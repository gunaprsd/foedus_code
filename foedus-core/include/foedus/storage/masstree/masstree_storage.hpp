/*
 * Copyright (c) 2014, Hewlett-Packard Development Company, LP.
 * The license and distribution terms for this file are placed in LICENSE.txt.
 */
#ifndef FOEDUS_STORAGE_MASSTREE_MASSTREE_STORAGE_HPP_
#define FOEDUS_STORAGE_MASSTREE_MASSTREE_STORAGE_HPP_

#include <iosfwd>
#include <string>

#include "foedus/attachable.hpp"
#include "foedus/cxx11.hpp"
#include "foedus/fwd.hpp"
#include "foedus/initializable.hpp"
#include "foedus/storage/fwd.hpp"
#include "foedus/storage/storage.hpp"
#include "foedus/storage/storage_id.hpp"
#include "foedus/storage/masstree/fwd.hpp"
#include "foedus/storage/masstree/masstree_id.hpp"
#include "foedus/thread/fwd.hpp"

namespace foedus {
namespace storage {
namespace masstree {
/**
 * @brief Represents a Masstree storage.
 * @ingroup MASSTREE
 */
class MasstreeStorage CXX11_FINAL
  : public virtual Storage, public Attachable<MasstreeStorageControlBlock> {
 public:
  MasstreeStorage() : Attachable<MasstreeStorageControlBlock>() {}
  /**
   * Constructs an masstree storage either from disk or newly create.
   */
  MasstreeStorage(Engine* engine, MasstreeStorageControlBlock* control_block)
    : Attachable<MasstreeStorageControlBlock>(engine, control_block) {
    ASSERT_ND(get_type() == kMasstreeStorage || !exists());
  }
  MasstreeStorage(Engine* engine, StorageControlBlock* control_block)
    : Attachable<MasstreeStorageControlBlock>(
      engine,
      reinterpret_cast<MasstreeStorageControlBlock*>(control_block)) {
    ASSERT_ND(get_type() == kMasstreeStorage || !exists());
  }
  /** Shorthand for engine->get_storage_manager()->get_masstree(name) */
  MasstreeStorage(Engine* engine, const StorageName& name);
  MasstreeStorage(const MasstreeStorage& other)
    : Attachable<MasstreeStorageControlBlock>(other.engine_, other.control_block_) {
  }
  MasstreeStorage& operator=(const MasstreeStorage& other) {
    engine_ = other.engine_;
    control_block_ = other.control_block_;
    return *this;
  }

  // Storage interface
  StorageId           get_id()    const CXX11_OVERRIDE;
  StorageType         get_type()  const CXX11_OVERRIDE;
  const StorageName&  get_name()  const CXX11_OVERRIDE;
  const Metadata*     get_metadata()  const CXX11_OVERRIDE;
  const MasstreeMetadata*  get_masstree_metadata()  const;
  bool                exists()    const CXX11_OVERRIDE;
  ErrorStack          create(const Metadata &metadata) CXX11_OVERRIDE;
  ErrorStack          drop() CXX11_OVERRIDE;
  void       describe(std::ostream* o) const CXX11_OVERRIDE;


  /**
   * @brief Prefetch data pages in this storage. Key Slice version (from/to are 8 bytes or less).
   * @param[in] context Thread context.
   * @param[in] from inclusive begin slice of records that are specifically prefetched even in
   * data pages.
   * @param[in] to inclusive end slice of records that are specifically prefetched even in data
   * pages.
   * @details
   * This is to \e warmup the storage for the current core.
   * Data pages are prefetched within from/to.
   * So far prefetches only volatile pages, but it will also cache and prefetch snapshot pages.
   */
  ErrorCode prefetch_pages_normalized(
    thread::Thread* context,
    KeySlice from = kInfimumSlice,
    KeySlice to = kSupremumSlice);

  // TODO(Hideaki) implement non key-slice version of prefetch_pages. so far this is enough, tho.

  // this storage type does use moved bit. so this is implemented
  bool                track_moved_record(xct::WriteXctAccess* write) CXX11_OVERRIDE;
  xct::LockableXctId* track_moved_record(xct::LockableXctId* address) CXX11_OVERRIDE;

  //// Masstree API

  // get_record() methods

  /**
   * @brief Retrieves an entire record of the given key in this Masstree.
   * @param[in] context Thread context
   * @param[in] key Arbitrary length of key that is lexicographically (big-endian) evaluated.
   * @param[in] key_length Byte size of key.
   * @param[out] payload Buffer to receive the payload of the record.
   * @param[in,out] payload_capacity [In] Byte size of the payload buffer, [Out] length of
   * the payload. This is set whether the payload capacity was too small or not.
   * @details
   * When payload_capacity is smaller than the actual payload, this method returns
   * kErrorCodeStrTooSmallPayloadBuffer and payload_capacity is set to be the required length.
   *
   * When the key is not found (kErrorCodeStrKeyNotFound), we also add an appropriate
   * record to \e range-lock read set because it is part of a transactional information.
   */
  ErrorCode   get_record(
    thread::Thread* context,
    const void* key,
    uint16_t key_length,
    void* payload,
    uint16_t* payload_capacity);

  /**
   * @brief Retrieves a part of the given key in this Masstree.
   * @param[in] context Thread context
   * @param[in] key Arbitrary length of key that is lexicographically (big-endian) evaluated.
   * @param[in] key_length Byte size of key.
   * @param[out] payload Buffer to receive the payload of the record.
   * @param[in] payload_offset We copy from this byte position of the record.
   * @param[in] payload_count How many bytes we copy.
   * @pre payload_offset + payload_count must be within the record's actual payload size
   * (returns kErrorCodeStrTooShortPayload if not)
   */
  ErrorCode   get_record_part(
    thread::Thread* context,
    const void* key,
    uint16_t key_length,
    void* payload,
    uint16_t payload_offset,
    uint16_t payload_count);

  /**
   * @brief Retrieves a part of the given key in this Masstree as a primitive value.
   * @param[in] context Thread context
   * @param[in] key Arbitrary length of key that is lexicographically (big-endian) evaluated.
   * @param[in] key_length Byte size of key.
   * @param[out] payload Receive the payload of the record.
   * @param[in] payload_offset We copy from this byte position of the record.
   * @pre payload_offset + sizeof(PAYLOAD) must be within the record's actual payload size
   * (returns kErrorCodeStrTooShortPayload if not)
   * @tparam PAYLOAD primitive type of the payload. all integers and floats are allowed.
   */
  template <typename PAYLOAD>
  ErrorCode   get_record_primitive(
    thread::Thread* context,
    const void* key,
    uint16_t key_length,
    PAYLOAD* payload,
    uint16_t payload_offset);

  /**
   * @brief Retrieves an entire record of the given primitive key in this Masstree.
   * @param[in] context Thread context
   * @param[in] key Primitive key that is evaluated in the primitive type's comparison rule.
   * @param[out] payload Buffer to receive the payload of the record.
   * @param[in,out] payload_capacity [In] Byte size of the payload buffer, [Out] length of
   * the payload. This is set whether the payload capacity was too small or not.
   */
  ErrorCode   get_record_normalized(
    thread::Thread* context,
    KeySlice key,
    void* payload,
    uint16_t* payload_capacity);

  /**
   * @brief Retrieves a part of the given primitive key in this Masstree.
   * @see get_record_part()
   * @see get_record_normalized()
   */
  ErrorCode   get_record_part_normalized(
    thread::Thread* context,
    KeySlice key,
    void* payload,
    uint16_t payload_offset,
    uint16_t payload_count);

  /**
   * @brief Retrieves a part of the given primitive key in this Masstree as a primitive value.
   * @see get_record_normalized()
   * @see get_record_primitive()
   */
  template <typename PAYLOAD>
  ErrorCode   get_record_primitive_normalized(
    thread::Thread* context,
    KeySlice key,
    PAYLOAD* payload,
    uint16_t payload_offset);

  // insert_record() methods

  /**
   * @brief Inserts a new record of the given key in this Masstree.
   * @param[in] context Thread context
   * @param[in] key Arbitrary length of key that is lexicographically (big-endian) evaluated.
   * @param[in] key_length Byte size of key.
   * @param[in] payload Value to insert.
   * @param[in] payload_count Length of payload.
   * @details
   * If the key already exists, it returns kErrorCodeStrKeyAlreadyExists and also adds the
   * found record to read set because it is part of transactional information.
   */
  ErrorCode   insert_record(
    thread::Thread* context,
    const void* key,
    uint16_t key_length,
    const void* payload,
    uint16_t payload_count);

  /**
   * @brief Inserts a new record without payload of the given key in this Masstree.
   * @param[in] context Thread context
   * @param[in] key Arbitrary length of key that is lexicographically (big-endian) evaluated.
   * @param[in] key_length Byte size of key.
   * @details
   * If the key already exists, it returns kErrorCodeStrKeyAlreadyExists and also adds the
   * found record to read set because it is part of transactional information.
   */
  ErrorCode   insert_record(
    thread::Thread* context,
    const void* key,
    uint16_t key_length) ALWAYS_INLINE {
      return insert_record(context, key, key_length, CXX11_NULLPTR, 0U);
  }

  /**
   * @brief Inserts a new record of the given primitive key in this Masstree.
   * @param[in] context Thread context
   * @param[in] key Primitive key that is evaluated in the primitive type's comparison rule.
   * @param[in] payload Value to insert.
   * @param[in] payload_count Length of payload.
   */
  ErrorCode   insert_record_normalized(
    thread::Thread* context,
    KeySlice key,
    const void* payload,
    uint16_t payload_count);

  /**
   * @brief Inserts a new record without payload of the given primitive key in this Masstree.
   * @param[in] context Thread context
   * @param[in] key Primitive key that is evaluated in the primitive type's comparison rule.
   */
  ErrorCode   insert_record_normalized(thread::Thread* context, KeySlice key) ALWAYS_INLINE {
    return insert_record_normalized(context, key, CXX11_NULLPTR, 0U);
  }

  // delete_record() methods

  /**
   * @brief Deletes a record of the given key from this Masstree.
   * @param[in] context Thread context
   * @param[in] key Arbitrary length of key that is lexicographically (big-endian) evaluated.
   * @param[in] key_length Byte size of key.
   * @details
   * When the key does not exist, it returns kErrorCodeStrKeyNotFound and also adds an appropriate
   * record to \e range-lock read set because it is part of transactional information.
   */
  ErrorCode   delete_record(thread::Thread* context, const void* key, uint16_t key_length);

  /**
   * @brief Deletes a record of the given primitive key from this Masstree.
   * @param[in] context Thread context
   * @param[in] key Primitive key that is evaluated in the primitive type's comparison rule.
   */
  ErrorCode   delete_record_normalized(thread::Thread* context, KeySlice key);

  // overwrite_record() methods

  /**
   * @brief Overwrites a part of one record of the given key in this Masstree.
   * @param[in] context Thread context
   * @param[in] key Arbitrary length of key that is lexicographically (big-endian) evaluated.
   * @param[in] key_length Byte size of key.
   * @param[in] payload We copy from this buffer. Must be at least payload_count.
   * @param[in] payload_offset We overwrite to this byte position of the record.
   * @param[in] payload_count How many bytes we overwrite.
   * @details
   * When payload_offset+payload_count is larger than the actual payload, this method returns
   * kErrorCodeStrTooShortPayload. Just like get_record(), this adds to range-lock read set
   * even when key is not found.
   */
  ErrorCode   overwrite_record(
    thread::Thread* context,
    const void* key,
    uint16_t key_length,
    const void* payload,
    uint16_t payload_offset,
    uint16_t payload_count);

  /**
   * @brief Overwrites a part of one record of the given key in this Masstree as a primitive value.
   * @param[in] context Thread context
   * @param[in] key Arbitrary length of key that is lexicographically (big-endian) evaluated.
   * @param[in] key_length Byte size of key.
   * @param[in] payload We copy this value.
   * @param[in] payload_offset We overwrite to this byte position of the record.
   * @pre payload_offset + sizeof(PAYLOAD) must be within the record's actual payload size
   * (returns kErrorCodeStrTooShortPayload if not)
   * @tparam PAYLOAD primitive type of the payload. all integers and floats are allowed.
   */
  template <typename PAYLOAD>
  ErrorCode   overwrite_record_primitive(
    thread::Thread* context,
    const void* key,
    uint16_t key_length,
    PAYLOAD payload,
    uint16_t payload_offset);

  /**
   * @brief Overwrites a part of one record of the given primitive key in this Masstree.
   * @param[in] context Thread context
   * @param[in] key Primitive key that is evaluated in the primitive type's comparison rule.
   * @param[in] payload We copy from this buffer. Must be at least payload_count.
   * @param[in] payload_offset We overwrite to this byte position of the record.
   * @param[in] payload_count How many bytes we overwrite.
   * @see get_record_normalized()
   */
  ErrorCode   overwrite_record_normalized(
    thread::Thread* context,
    KeySlice key,
    const void* payload,
    uint16_t payload_offset,
    uint16_t payload_count);

  /**
   * @brief Overwrites a part of one record of the given primitive key
   * in this Masstree as a primitive value.
   * @see overwrite_record_primitive()
   * @see overwrite_record_normalized()
   */
  template <typename PAYLOAD>
  ErrorCode   overwrite_record_primitive_normalized(
    thread::Thread* context,
    KeySlice key,
    PAYLOAD payload,
    uint16_t payload_offset);


  // increment_record() methods

  /**
   * @brief This one further optimizes overwrite methods for the frequent use
   * case of incrementing some data in primitive type.
   * @param[in] context Thread context
   * @param[in] key Arbitrary length of key that is lexicographically (big-endian) evaluated.
   * @param[in] key_length Byte size of key.
   * @param[in,out] value (in) addendum, (out) value after addition.
   * @param[in] payload_offset We overwrite to this byte position of the record.
   * @pre payload_offset + sizeof(PAYLOAD) must be within the record's actual payload size
   * (returns kErrorCodeStrTooShortPayload if not)
   * @tparam PAYLOAD primitive type of the payload. all integers and floats are allowed.
   */
  template <typename PAYLOAD>
  ErrorCode   increment_record(
    thread::Thread* context,
    const void* key,
    uint16_t key_length,
    PAYLOAD* value,
    uint16_t payload_offset);

  /**
   * @brief For primitive key.
   * @see increment_record()
   */
  template <typename PAYLOAD>
  ErrorCode   increment_record_normalized(
    thread::Thread* context,
    KeySlice key,
    PAYLOAD* value,
    uint16_t payload_offset);

  // TODO(Hideaki): Extend/shrink/update methods for payload. A bit faster than delete + insert.


  ErrorStack  verify_single_thread(thread::Thread* context);
};
}  // namespace masstree
}  // namespace storage
}  // namespace foedus
#endif  // FOEDUS_STORAGE_MASSTREE_MASSTREE_STORAGE_HPP_
