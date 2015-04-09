/*
 * Copyright (c) 2014-2015, Hewlett-Packard Development Company, LP.
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details. You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * HP designates this particular file as subject to the "Classpath" exception
 * as provided by HP in the LICENSE.txt file that accompanied this code.
 */
#ifndef FOEDUS_STORAGE_HASH_HASH_ID_HPP_
#define FOEDUS_STORAGE_HASH_HASH_ID_HPP_
#include <stdint.h>

#include <iosfwd>

#include "foedus/storage/storage_id.hpp"

/**
 * @file foedus/storage/hash/hash_id.hpp
 * @brief Definitions of IDs in this package and a few related constant values.
 * @ingroup HASH
 */
namespace foedus {
namespace storage {
namespace hash {

/**
 * @brief Byte size of header in root page of hash storage.
 * @ingroup HASH
 */
const uint16_t kHashRootPageHeaderSize  = 32 + 8 * 2;

/**
 * @brief Number of pointers in a root page of hash storage.
 * @ingroup HASH
 */
const uint16_t kHashRootPageFanout      =
  (kPageSize - kHashRootPageHeaderSize) / sizeof(DualPagePointer);

/**
 * @brief Byte size of header in an intermediate page of hash storage.
 * @ingroup HASH
 */
const uint16_t kHashIntermediatePageHeaderSize  = 32 + 8 * 2;

/**
 * @brief Number of pointers in an intermediate page of hash storage.
 * @ingroup HASH
 */
const uint16_t kHashIntermediatePageFanout      =
  (kPageSize - kHashIntermediatePageHeaderSize) / sizeof(DualPagePointer);

/**
 * @brief Byte size of header in bin page of hash storage.
 * @ingroup HASH
 */
const uint16_t kHashBinPageHeaderSize   = 64;

/**
 * @brief Byte size of header in data page of hash storage.
 * @ingroup HASH
 */
const uint16_t kHashDataPageHeaderSize  = 256;

/**
 * @brief Represents a full 64-bit hash value calculated from a key.
 * @ingroup HASH
 * @details
 * This value is usually split into two parts, higher bits as \e bins and other bits as \e tag.
 * Each hash storage has a static configuration that determines how many bits are used for bins.
 * Each bin represents a range of hash values, such as 0x1234000000000000 (inclusive)
 * to 0x1235000000000000 (exclusive) where bins use the high 16 bits and tags use low 48 bits.
 * HashTag, which is always 16 bits, is calculated by collapsing low 48 bits.
 * @see HashTag
 * @see HashRange
 */
typedef uint64_t HashValue;

/**
 * @brief Represents a range of hash values in a hash storage, such as what an intermediate page
 * is responsible for.
 * @ingroup HASH
 * @details
 * Begin is inclusive, end is exclusive.
 */
struct HashRange {
  HashRange() : begin_(0), end_(0) {}
  HashRange(HashValue begin, HashValue end) : begin_(begin), end_(end) {}

  /** Returns if there is any overlap with the other range. */
  bool    overlaps(const HashRange& other) const {
    // Case 1: contains(other.begin) or contains(other.end)
    // Case 2: not case 1, but other.contains(begin)
    return contains(other.begin_) || contains(other.end_) || other.contains(begin_);
  }
  bool    contains(HashValue hash) const { return hash >= begin_ && hash < end_; }
  bool    operator==(const HashRange& other) const {
    return begin_ == other.begin_ && end_ == other.end_;
  }
  bool    operator!=(const HashRange& other) const { return !(this->operator==(other)); }

  /** Inclusive beginning of the hash range. */
  HashValue begin_;
  /** Exclusive end of the hash range. */
  HashValue end_;
};


/**
 * @brief Represents a compact \e tag of hash values.
 * @ingroup HASH
 * @details
 * This value is used to efficiently differentiate entries placed in the same hash bin
 * and also to calculate the alternative hash bin. For more details, see [FAN13].
 * The larger this type is, the more entries per bin we can differentiate at the cost of
 * larger space in bin pages. Due to the 16 bytes overhead (DualPagePointer) per bin,
 * our bin stores a relatively large number of entries, so we picked 2 bytes rather than 1 byte.
 */
typedef uint16_t HashTag;


/**
 * @brief Byte size of one hash bin.
 * @ingroup HASH
 */
const uint16_t kHashBinSize             = 64;

/**
 * @brief Max number of entries in one hash bin.
 * @ingroup HASH
 */
const uint16_t kMaxEntriesPerBin        =
  (kHashBinSize - sizeof(DualPagePointer) - sizeof(uint16_t)) / sizeof(HashTag);

/**
 * @brief Number of bins in one hash bin page.
 * @ingroup HASH
 */
const uint16_t kBinsPerPage             = (kPageSize - kHashBinPageHeaderSize) / kHashBinSize;

}  // namespace hash
}  // namespace storage
}  // namespace foedus
#endif  // FOEDUS_STORAGE_HASH_HASH_ID_HPP_
