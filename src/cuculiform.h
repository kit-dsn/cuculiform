#pragma once

#include <algorithm>
#include <assert.h>
#include <functional>
#include <iostream>
#include <iterator>
#include <random>
#include <tuple>
#include <vector>

#include "bucket.h"
#include "fingerprint.h"
#include "util.h"

namespace cuculiform {

template <typename T>
class CuckooFilter {
public:
  explicit CuckooFilter(size_t capacity, size_t fingerprint_size,
                        uint max_relocations = 500,
                        std::function<uint64_t(size_t)> cuckoo_hash_fn =
                          cuculiform::TwoIndependentMultiplyShift{},
                        std::function<uint64_t(size_t)> fingerprint_hash_fn =
                          cuculiform::TwoIndependentMultiplyShift{})
      : m_size(0),
        m_capacity(capacity),
        m_bucket_size(4),
        // for partial hashing to work, i.e. not generate invalid bucket
        // indexes.
        m_bucket_count(ceil_to_power_of_two(m_capacity / m_bucket_size)),
        m_fingerprint_size(fingerprint_size),
        m_max_relocations(max_relocations),
        m_cuckoo_hash_fn(cuckoo_hash_fn),
        m_fingerprint_hash_fn(fingerprint_hash_fn),
        index_dis(0, 1),
        bucket_dis(0, m_bucket_size - 1) {
    assert(m_fingerprint_size > 0);
    assert(m_fingerprint_size <= 4);

    m_data = std::vector<uint8_t>(m_capacity * fingerprint_size,
                                  static_cast<uint8_t>(0));

    // Will be used to obtain a seed for the random number engine
    std::random_device rd;
    auto seed = rd();
    /* auto seed = 3377404304; */
    /* std::cerr << "seed: " << seed << std::endl; */
    // Standard mersenne_twister_engine seeded with rd()
    gen = std::mt19937(seed);
  }

  bool insert(const T item);
  bool contains(const T item) const;
  bool erase(const T item);
  void clear();
  size_t size() const;
  size_t capacity() const;
  size_t memory_usage() const;
  void memory_usage_info() const;

  template <typename U>
  friend std::ostream& operator<<(std::ostream& out,
                                  const CuckooFilter<U>& filter);

private:
  size_t m_size;
  std::vector<uint8_t> m_data;
  const size_t m_capacity;     // total number of fingerprints in the filter
  const size_t m_bucket_size;  // number of fingerprints that fit in a bucket
  const size_t m_bucket_count; // number of buckets in the filter
  const size_t m_fingerprint_size; // size of the fingerprint in bytes
  const uint m_max_relocations;    // max number of relocations before filled
  const std::function<uint64_t(size_t)>
    m_cuckoo_hash_fn; // hash function used for partial cuckoo hashing
  const std::function<uint64_t(size_t)>
    m_fingerprint_hash_fn; // hash function used for fingerprinting
  std::mt19937 gen;        // Standard mersenne_twister_engine seeded with rd()
  std::uniform_int_distribution<> index_dis;
  std::uniform_int_distribution<> bucket_dis;

  size_t get_alt_index(const size_t index,
                       const uint32_t fingerprint_linear) const;
  size_t get_alt_index(const size_t index, const Fingerprint fingerprint) const;
  std::tuple<size_t, size_t, Fingerprint>
  get_indexes_and_fingerprint_for(const T item) const;

  Bucket get_bucket(const size_t index) {
    return Bucket(
      std::next(m_data.begin(), index * m_bucket_size * m_fingerprint_size),
      std::next(m_data.begin(),
                (index + 1) * m_bucket_size * m_fingerprint_size),
      m_fingerprint_size);
  }
  const Bucket get_bucket(const size_t index) const {
    // yes this is cheating around not being able to create a Bucket with const
    // iterators, but should work because we return a const bucket
    auto& data_noconst = const_cast<std::vector<uint8_t>&>(m_data);
    return Bucket(std::next(data_noconst.begin(),
                            index * m_bucket_size * m_fingerprint_size),
                  std::next(data_noconst.begin(),
                            (index + 1) * m_bucket_size * m_fingerprint_size),
                  m_fingerprint_size);
  }
};

template <typename T>
inline size_t
CuckooFilter<T>::get_alt_index(const size_t index,
                               const uint32_t fingerprint_linear) const {
  size_t alt_index =
    index
    ^ (static_cast<uint32_t>(m_cuckoo_hash_fn(fingerprint_linear))
       % m_bucket_count);

  return alt_index;
}

template <typename T>
inline size_t
CuckooFilter<T>::get_alt_index(const size_t index,
                               const Fingerprint fingerprint) const {
  uint32_t fingerprint_linear = from_bytes(fingerprint);
  return get_alt_index(index, fingerprint_linear);
}

template <typename T>
inline std::tuple<size_t, size_t, Fingerprint>
CuckooFilter<T>::get_indexes_and_fingerprint_for(const T item) const {
  // use std::hash to normalize any type to a size_t.
  // Note that it doesn't necessarily produce distributed hashes,
  // i.e. for uints, it might just be the identity function.
  // To have an equally distributed hash, we then apply a chosen hash function.
  std::hash<T> weak_hash_fn;
  uint64_t item_hash = weak_hash_fn(item);
  uint64_t cuckoo_hash = m_cuckoo_hash_fn(item_hash);
  uint64_t fingerprint = m_fingerprint_hash_fn(item_hash);

  // only use fingerprint_size bytes of the hash
  fingerprint = fingerprint >> (sizeof(fingerprint) - m_fingerprint_size) * 8;
  if (fingerprint == 0) {
    fingerprint = 1;
  }

  // Apply % m_bucket_count now and not later on operation execution.
  // If done later, this is probably the cause for items "vanishing", which,
  // turns out, actually means they're inserted in the wrong bucket on
  // relocation and therefore are not found when being checked after them. This
  // happens because get_alt_index returns other values when inserting vs. when
  // relocating, because relocation uses the index of the element that gets
  // relocated instead of computing it from the actual element. If applying
  // modulo early / having a m_bucket_count which is a power of two, those
  // indexes are equivalent. Strange: In contrast to the reference
  // implementation, the rust implementation applies the modulo on operation
  // execution and apparently does work as well.
  size_t index = cuckoo_hash % m_bucket_count;
  size_t alt_index = get_alt_index(index, fingerprint);

  std::vector<uint8_t> fingerprint_vec =
    into_bytes(fingerprint, m_fingerprint_size);
  assert(index == get_alt_index(alt_index, fingerprint_vec));

  return std::make_tuple(index, alt_index, fingerprint_vec);
}

template <typename T>
inline bool CuckooFilter<T>::insert(const T item) {
  size_t index;
  size_t alt_index;
  Fingerprint fingerprint;

  std::tie(index, alt_index, fingerprint) =
    get_indexes_and_fingerprint_for(item);
  assert(index == get_alt_index(alt_index, fingerprint));

  // TODO: insert two times the same value?

  size_t index_to_insert = index_dis(gen) ? index : alt_index;
  bool inserted = get_bucket(index_to_insert).insert(fingerprint);
  if (inserted) {
    m_size++;
    return true;
  }

  index_to_insert = get_alt_index(index_to_insert, fingerprint);
  for (uint i = 0; i < m_max_relocations; i++) {
    auto bucket = get_bucket(index_to_insert);
    bool inserted = bucket.insert(fingerprint);
    if (inserted) {
      m_size++;
      return true;
    } else {
      size_t fingerprint_to_relocate = bucket_dis(gen);
      bucket.swap(fingerprint, fingerprint_to_relocate);

      index_to_insert = get_alt_index(index_to_insert, fingerprint);
    }
  }

  // TODO: have a victim cache like the reference implementation instead of
  // throwing the last element out?
  return false;
}

template <typename T>
inline bool CuckooFilter<T>::contains(const T item) const {
  size_t index;
  size_t alt_index;
  std::vector<uint8_t> fingerprint;
  std::tie(index, alt_index, fingerprint) =
    get_indexes_and_fingerprint_for(item);

  assert(alt_index == get_alt_index(index, fingerprint));
  assert(index == get_alt_index(alt_index, fingerprint));

  bool contained = get_bucket(index).contains(fingerprint)
                   || get_bucket(alt_index).contains(fingerprint);
  return contained;
}

template <typename T>
inline bool CuckooFilter<T>::erase(const T item) {
  size_t index;
  size_t alt_index;
  std::vector<uint8_t> fingerprint;
  std::tie(index, alt_index, fingerprint) =
    get_indexes_and_fingerprint_for(item);

  // TODO: Same element removed two times?
  bool erased = get_bucket(index).erase(fingerprint)
                || get_bucket(alt_index).erase(fingerprint);
  if (erased) {
    m_size--;
  }
  return erased;
}

template <typename T>
inline void CuckooFilter<T>::clear() {
  std::fill(std::begin(m_data), std::end(m_data), 0);
  m_size = 0;
}

template <typename T>
inline size_t CuckooFilter<T>::size() const {
  return m_size;
}

template <typename T>
inline size_t CuckooFilter<T>::capacity() const {
  return m_capacity;
}

template <typename T>
inline size_t CuckooFilter<T>::memory_usage() const {
  return sizeof(CuckooFilter<T>) + sizeof(uint8_t) * m_data.size();
}

template <typename T>
inline void CuckooFilter<T>::memory_usage_info() const {
  std::cerr << "== CuckooFilter memory usage broken up: ==" << std::endl;
  std::cerr << "sizeof CuckooFilter struct: " << sizeof(CuckooFilter<T>) << "B"
            << std::endl;
  std::cerr << "number of buckets: " << m_bucket_count << std::endl;
  std::cerr << "single bucket memory usage: "
            << sizeof(uint8_t) * m_bucket_size * m_fingerprint_size << "B"
            << std::endl;
  std::cerr << "==========================================" << std::endl;
}

template <typename T>
inline std::ostream& operator<<(std::ostream& out,
                                const CuckooFilter<T>& filter) {
  out << "{" << std::hex << std::endl;
  for (size_t bucket_index = 0; bucket_index < filter.m_bucket_count;
       bucket_index++) {
    out << "  {";
    for (size_t fingerprint_index = 0; fingerprint_index < filter.m_bucket_size;
         fingerprint_index++) {
      for (size_t byte_index = 0; byte_index < filter.m_fingerprint_size;
           byte_index++) {
        // cast from uint8_t to uint16_t to avoid garbage output because uint8_t
        // is a typedef for unsigned char and << is overloaded for chars to
        // print those values as characters. Seems like there is no other way.
        out << static_cast<uint16_t>(
          filter.m_data[bucket_index * filter.m_bucket_size
                          * filter.m_fingerprint_size
                        + fingerprint_index * filter.m_fingerprint_size
                        + byte_index]);
      }
      out << ", ";
    }
    out << "}," << std::endl;
  }
  out << "}" << std::dec << std::endl;
  return out;
}

} // namespace cuculiform
