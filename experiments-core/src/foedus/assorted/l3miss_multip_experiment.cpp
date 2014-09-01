/*
 * Copyright (c) 2014, Hewlett-Packard Development Company, LP.
 * The license and distribution terms for this file are placed in LICENSE.txt.
 */

// Tests the cost of L3 cache miss using child processes.
// Each NUMA node is spawned as a child process via fork(2).
#include <numa.h>
#include <numaif.h>
#include <unistd.h>
#include <sys/wait.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "foedus/assorted/assorted_func.hpp"
#include "foedus/assorted/uniform_random.hpp"
#include "foedus/debugging/stop_watch.hpp"
#include "foedus/memory/aligned_memory.hpp"
#include "foedus/memory/memory_id.hpp"
#include "foedus/thread/numa_thread_scope.hpp"

const uint64_t kMemory = 12ULL << 30;
const uint32_t kRep = 1ULL << 26;

int nodes;
int cores_per_node;
foedus::memory::AlignedMemory::AllocType alloc_type
  = foedus::memory::AlignedMemory::kNumaAllocOnnode;

struct ProcessChannel {
  std::atomic<int>  initialized_count;
  std::atomic<bool> experiment_started;
  std::atomic<int>  exit_count;
};
foedus::memory::AlignedMemory process_channel_memory;
ProcessChannel* process_channel;

foedus::memory::AlignedMemory* data_memories;

uint64_t run(const char* blocks, foedus::assorted::UniformRandom* rands) {
  uint64_t memory_per_core = kMemory / cores_per_node;
  uint64_t ret = 0;
  for (uint32_t i = 0; i < kRep; ++i) {
    const char* block = blocks + ((rands->next_uint32() % (memory_per_core >> 6)) << 6);
    block += ret % (1 << 6);
    ret += *block;
  }
  return ret;
}

void main_impl(int id, int node) {
  foedus::thread::NumaThreadScope scope(node);
  uint64_t memory_per_core = kMemory / cores_per_node;
  char* memory = reinterpret_cast<char*>(data_memories[node].get_block());
  memory += (memory_per_core * id);

  foedus::assorted::UniformRandom uniform_random(id);

  ++process_channel->initialized_count;
  while (process_channel->experiment_started.load() == false) {
    continue;
  }
  {
    foedus::debugging::StopWatch stop_watch;
    uint64_t ret = run(memory, &uniform_random);
    stop_watch.stop();
    std::stringstream str;
    str << "Done " << node << "-" << id
      << " (ret=" << ret << ") in " << stop_watch.elapsed_ms() << " ms. "
      << "On average, " << (static_cast<double>(stop_watch.elapsed_ns()) / kRep)
      << " ns/miss" << std::endl;
    std::cout << str.str();
  }
}
int process_main(int node) {
  std::cout << "Node-" << node << " started working on pid-" << ::getpid() << std::endl;
  /*
  {
    nodemask_t mask;
    ::nodemask_zero(&mask);
    ::nodemask_set_compat(&mask, node);
    bitmask mask2;
    ::copy_nodemask_to_bitmask(&mask, &mask2);
    ::numa_bind(&mask2);
  }
  */
  foedus::thread::NumaThreadScope scope(node);
  std::vector<std::thread> threads;
  for (auto i = 0; i < cores_per_node; ++i) {
    threads.emplace_back(std::thread(main_impl, i, node));
  }
  std::cout << "Node-" << node << " launched " << cores_per_node << " threads" << std::endl;
  for (auto& t : threads) {
    t.join();
  }
  std::cout << "Node-" << node << " ended normally" << std::endl;
  ++process_channel->exit_count;
  return 0;
}
void data_alloc(int node) {
  data_memories[node].alloc(kMemory, 1ULL << 30, alloc_type, node, true);
  std::cout << "Allocated memory for node-" << node << ":"
    << data_memories[node].get_block() << std::endl;
}

int main(int argc, char **argv) {
  if (argc < 3) {
    std::cerr << "Usage: ./l3miss_multip_experiment <nodes> <cores_per_node> [<use_mmap>]"
      << std::endl;
    return 1;
  }
  nodes = std::atoi(argv[1]);
  if (nodes == 0 || nodes > ::numa_num_configured_nodes()) {
    std::cerr << "Invalid <nodes>:" << argv[1] << std::endl;
    return 1;
  }
  cores_per_node = std::atoi(argv[2]);
  if (cores_per_node == 0 ||
    cores_per_node > (::numa_num_configured_cpus() / ::numa_num_configured_nodes())) {
    std::cerr << "Invalid <cores_per_node>:" << argv[2] << std::endl;
    return 1;
  }
  if (argc >= 4 && std::string(argv[3]) != std::string("false")) {
    alloc_type = foedus::memory::AlignedMemory::kNumaMmapOneGbPages;
  }

  std::cout << "Allocating data memory.." << std::endl;
  data_memories = new foedus::memory::AlignedMemory[nodes];
  {
    std::vector<std::thread> alloc_threads;
    for (auto node = 0; node < nodes; ++node) {
      alloc_threads.emplace_back(data_alloc, node);
    }
    for (auto& t : alloc_threads) {
      t.join();
    }
  }
  std::cout << "Allocated all data memory." << std::endl;

  process_channel_memory.alloc(
    1 << 21,
    1 << 21,
    foedus::memory::AlignedMemory::kNumaAllocOnnode,
    0,
    true);
  process_channel = reinterpret_cast<ProcessChannel*>(process_channel_memory.get_block());
  process_channel->initialized_count = 0;
  process_channel->experiment_started = false;
  process_channel->exit_count = 0;

  std::vector<pid_t> pids;
  std::vector<bool> exitted;
  for (auto node = 0; node < nodes; ++node) {
    pid_t pid = ::fork();
    if (pid == -1) {
      std::cerr << "fork() failed, error=" << foedus::assorted::os_error() << std::endl;
      return 1;
    }
    if (pid == 0) {
      return process_main(node);
    } else {
      // parent
      std::cout << "child process-" << pid << " has been forked" << std::endl;
      pids.push_back(pid);
      exitted.push_back(false);
    }
  }
  while (process_channel->initialized_count < (nodes * cores_per_node)) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  std::cout << "Child initialization done! Starts the experiment..." << std::endl;
  process_channel->experiment_started.store(true);

  while (process_channel->exit_count < nodes) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Waiting for end... exit_count=" << process_channel->exit_count << std::endl;
    for (uint16_t i = 0; i < pids.size(); ++i) {
      if (exitted[i]) {
        continue;
      }
      pid_t pid = pids[i];
      int status;
      pid_t result = ::waitpid(pid, &status, WNOHANG);
      if (result == 0) {
        std::cout << "  pid-" << pid << " is still alive.." << std::endl;
      } else if (result == -1) {
        std::cout << "  pid-" << pid << " had an error! quit" << std::endl;
        return 1;
      } else {
        std::cout << "  pid-" << pid << " has exit with status code " << status << std::endl;
        exitted[i] = true;
      }
    }
  }
  std::cout << "All done!" << std::endl;
  delete[] data_memories;
  return 0;
}
