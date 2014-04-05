/*
 * Copyright (c) 2014, Hewlett-Packard Development Company, LP.
 * The license and distribution terms for this file are placed in LICENSE.txt.
 */
#ifndef FOEDUS_MEMORY_NUMA_NODE_MEMORY_HPP_
#define FOEDUS_MEMORY_NUMA_NODE_MEMORY_HPP_
#include <foedus/cxx11.hpp>
#include <foedus/error_stack.hpp>
#include <foedus/initializable.hpp>
namespace foedus {
namespace memory {
/**
 * @brief Repository of memories dynamically acquired and shared within one NUMA node (socket).
 * @ingroup MEMHIERARCHY
 * @details
 * Detailed description of this class.
 */
class NumaNodeMemory : public virtual Initializable {
 public:
    /**
     * Description of constructor.
     */
    NumaNodeMemory();
    /**
     * Description of destructor.
     */
    ~NumaNodeMemory();

    ErrorStack initialize() CXX11_OVERRIDE;
    bool is_initialized() const CXX11_OVERRIDE { return initialized_; }
    ErrorStack uninitialize() CXX11_OVERRIDE;
 private:
    bool    initialized_;
};
}  // namespace memory
}  // namespace foedus
#endif  // FOEDUS_MEMORY_NUMA_NODE_MEMORY_HPP_