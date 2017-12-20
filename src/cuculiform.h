#pragma once

#include <algorithm>
#include <assert.h>
#include <functional>
#include <random>

#include "highwayhash/highwayhash.h"

using namespace highwayhash;

namespace cuculiform {

// round to next highest power of two of 64bit v
// https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
inline size_t ceil_to_power_of_two(size_t v) {
  v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v |= v >> 32;
  v++;
  v += (v == 0);
  return v;
}

// convert byte vector to uint32_t representation
inline uint32_t from_bytes(std::vector<uint8_t> vec) {
  uint32_t linear = 0;
  for (size_t i = 0; i < vec.size(); i++) {
    linear |= (vec[i] << i * 8);
  }
  return linear;
}
// convert a uint32_t into byte vector representation
inline std::vector<uint8_t> into_bytes(uint32_t linear, size_t num_bytes) {
  std::vector<uint8_t> vec(num_bytes);
  for (size_t i = 0; i < vec.size(); i++) {
    // shift to right to remove everything unneeded
    vec[i] = static_cast<uint8_t>(linear >> i * 8);
  }
  return vec;
}

// implements the required strong hash function signature
// for CuckooFilter using HighwayHash
inline uint64_t highwayhash(size_t value) {
  char bytes[sizeof(value)];
  for (size_t i = 0; i < sizeof(value); i++) {
    bytes[i] = static_cast<uint8_t>((value >> i * 8) & 0xFF);
  }

  const HHKey key HH_ALIGNAS(32) = {1, 2, 3, 4};
  HHResult64 result;
  HHStateT<HH_TARGET> state(key);
  HighwayHashT(&state, bytes, sizeof(bytes), &result);
  return static_cast<uint64_t>(result);
}

typedef std::vector<uint8_t> Fingerprint; // TODO: Use it?

struct Bucket {
  explicit Bucket(size_t bucket_size, size_t fingerprint_size) {
    auto empty_fingerprint = std::vector<uint8_t>(fingerprint_size, 0);
    m_fingerprints =
      std::vector<std::vector<uint8_t>>(bucket_size, empty_fingerprint);
  }

  std::vector<std::vector<uint8_t>> m_fingerprints;

  bool insert(const std::vector<uint8_t> fingerprint);
  void swap(std::vector<uint8_t>& fingerprint, size_t index);
  bool contains(const std::vector<uint8_t> fingerprint) const;
  bool erase(const std::vector<uint8_t> fingerprint);
  void clear();

  size_t memory_usage() const;
  void memory_usage_info() const;

  friend std::ostream& operator<<(std::ostream& out, const Bucket& bucket);
};

inline bool Bucket::insert(const std::vector<uint8_t> fingerprint) {
  auto empty_fingerprint = std::vector<uint8_t>(fingerprint.size(), 0);
  assert(empty_fingerprint != fingerprint);
  auto position =
    std::find(m_fingerprints.begin(), m_fingerprints.end(), empty_fingerprint);
  bool has_empty_position = position != m_fingerprints.end();
  if (has_empty_position) {
    // found empty position, insert by bytewise-copying the fingerprint into
    // that slot
    std::copy(fingerprint.begin(), fingerprint.end(), position->begin());
  }
  return has_empty_position;
}

inline void Bucket::swap(std::vector<uint8_t>& fingerprint, size_t index) {
  std::swap(m_fingerprints[index], fingerprint);
}

inline bool Bucket::contains(const std::vector<uint8_t> fingerprint) const {
  auto position =
    std::find(m_fingerprints.begin(), m_fingerprints.end(), fingerprint);
  return position != m_fingerprints.end();
}

inline bool Bucket::erase(const std::vector<uint8_t> fingerprint) {
  auto position =
    std::find(m_fingerprints.begin(), m_fingerprints.end(), fingerprint);
  bool has_fingerprint = position != m_fingerprints.end();
  if (has_fingerprint) {
    // found that fingerprint, delete it by filling its slot with zeros
    std::fill(position->begin(), position->end(), 0);
  }
  return has_fingerprint;
}

inline void Bucket::clear() {
  std::for_each(m_fingerprints.begin(), m_fingerprints.end(),
                [](std::vector<uint8_t>& fingerprint) {
                  std::fill(fingerprint.begin(), fingerprint.end(), 0);
                });
}

inline size_t Bucket::memory_usage() const {
  // (sizeof actually takes array-like padding into account)
  // assert that all Buckets have the same memory usage:
  size_t reserved_fingerprint_size = sizeof(m_fingerprints[0]);
  size_t used_fingerprint_size =
    reserved_fingerprint_size + m_fingerprints[0].capacity() * sizeof(uint8_t);
  return sizeof(Bucket) + used_fingerprint_size * m_fingerprints.size()
         + reserved_fingerprint_size
             * (m_fingerprints.capacity() - m_fingerprints.size());
}

inline void Bucket::memory_usage_info() const {
  std::cerr << "== Bucket memory usage broken up: ==" << std::endl;
  std::cerr << "sizeof Bucket struct: " << sizeof(Bucket) << "B" << std::endl;
  std::cerr << "number of used fingerprints: " << m_fingerprints.size()
            << std::endl;
  std::cerr << "number of excess fingerprints: "
            << m_fingerprints.capacity() - m_fingerprints.size() << std::endl;
  std::cerr << "single used fingerprint memory usage: "
            << sizeof(m_fingerprints[0])
                 + m_fingerprints[0].capacity() * sizeof(uint8_t)
            << "B" << std::endl;
  std::cerr << "single unused fingerprint memory usage: "
            << sizeof(m_fingerprints[0]) << "B" << std::endl;
}

inline std::ostream& operator<<(std::ostream& out, const Bucket& bucket) {
  out << "{ " << std::hex;
  for (const auto& fingerprint : bucket.m_fingerprints) {
    out << from_bytes(fingerprint) << ", ";
  }
  out << "}" << std::dec;
  return out;
}

template <typename T>
class CuckooFilter {
public:
  explicit CuckooFilter(
    size_t capacity, size_t fingerprint_size, uint max_relocations = 10,
    std::function<uint64_t(size_t)> strong_hash_fn = highwayhash)
      : m_size(0),
        m_capacity(capacity),
        m_bucket_size(4),
        m_fingerprint_size(fingerprint_size),
        m_max_relocations(max_relocations),
        m_strong_hash_fn(strong_hash_fn),
        index_dis(0, 1),
        bucket_dis(0, m_bucket_size - 1) {
    assert(m_fingerprint_size > 0);
    assert(m_fingerprint_size <= 4);

    // for partial hashing to work, i.e. not generate invalid bucket indexes.
    size_t num_buckets = ceil_to_power_of_two(m_capacity / m_bucket_size);

    Bucket empty_bucket{m_bucket_size, m_fingerprint_size};
    m_buckets = std::vector<Bucket>(num_buckets, empty_bucket);

    // Will be used to obtain a seed for the random number engine
    std::random_device rd;
    auto seed = rd();
    /* auto seed = 1115641093; */
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
  std::vector<Bucket> m_buckets;
  const size_t m_capacity;
  const size_t m_bucket_size;
  const size_t m_fingerprint_size;
  const uint m_max_relocations;
  const std::function<uint64_t(size_t)> m_strong_hash_fn;
  std::mt19937 gen; // Standard mersenne_twister_engine seeded with rd()
  std::uniform_int_distribution<> index_dis;
  std::uniform_int_distribution<> bucket_dis;

  size_t get_alt_index(const size_t index, const Fingerprint fingerprint) const;
  std::tuple<size_t, size_t, Fingerprint>
  get_indexes_and_fingerprint_for(const T item) const;
};

template <typename T>
inline size_t
CuckooFilter<T>::get_alt_index(const size_t index,
                               const Fingerprint fingerprint) const {
  uint32_t fingerprint_linear = from_bytes(fingerprint);

  size_t alt_index =
    index
    ^ (static_cast<uint32_t>(m_strong_hash_fn(fingerprint_linear))
       % m_buckets.size());

  return alt_index;
}

template <typename T>
inline std::tuple<size_t, size_t, Fingerprint>
CuckooFilter<T>::get_indexes_and_fingerprint_for(const T item) const {
  // use std::hash to normalize any type to a size_t.
  // Note that it doesn't necessarily produce distributed hashes,
  // i.e. for uints, it might just be the identity function.
  // To have an equally distributed hash, we then apply a strong hash function.
  std::hash<T> weak_hash_fn;
  uint64_t hash = m_strong_hash_fn(weak_hash_fn(item));

  // split hash to lower and upper half
  const uint32_t fingerprint_part = static_cast<uint32_t>(hash);
  const uint32_t index_part = static_cast<uint32_t>(hash >> 32);

  // only use fingerprint_size bytes of the hash
  uint32_t fingerprint = fingerprint_part >> (32 - m_fingerprint_size * 8);
  if (fingerprint == 0) {
    fingerprint = 1;
    std::cerr << "Fingerprint collision with 0 on element " << item
              << std::endl;
  }
  assert(fingerprint != 0);

  std::vector<uint8_t> fingerprint_vec =
    into_bytes(fingerprint, m_fingerprint_size);

  // Apply % buckets.size() now and not later on operation execution.
  // If done later, this is probably the cause for items "vanishing", which,
  // turns out, actually means they're inserted in the wrong bucket on
  // relocation and therefore are not found when being checked after them. This
  // happens because get_alt_index returns other values when inserting vs. when
  // relocating, because relocation uses the index of the element that gets
  // relocated instead of computing it from the actual element. If applying
  // modulo early / having a buckets.size() wich is a power of two, those
  // indexes are equivalent. Strange: In contrast to the reference
  // implementation, the rust implementation applies the modulo on operation
  // execution and apparently does work as well.
  size_t index = index_part % m_buckets.size();
  size_t alt_index =
    get_alt_index(index, fingerprint_vec); // TODO: Additional conversion :(

  assert(index == get_alt_index(alt_index, fingerprint_vec));

  return std::make_tuple(index, alt_index, fingerprint_vec);
}

template <typename T>
inline bool CuckooFilter<T>::insert(const T item) {

  size_t index;
  size_t alt_index;
  std::vector<uint8_t> fingerprint;
  std::tie(index, alt_index, fingerprint) = get_indexes_and_fingerprint_for(item);

  assert(index == get_alt_index(alt_index, fingerprint));

  size_t index_to_insert = index_dis(gen) ? index : alt_index;
  bool inserted = m_buckets[index_to_insert].insert(fingerprint);
  if (inserted) {
    m_size++;
    return true;
  }

  // TODO: insert two times the same value?
  Fingerprint fp_to_insert = fingerprint;
  index_to_insert = get_alt_index(index_to_insert, fp_to_insert);
  for (uint i = 0; i < m_max_relocations; i++) {
    bool inserted = m_buckets[index_to_insert].insert(fp_to_insert);
    if (inserted) {
      m_size++;
      return true;
    } else {

      size_t which_fingerprint_to_relocate = bucket_dis(gen);

      m_buckets[index_to_insert].swap(fp_to_insert,
                                      which_fingerprint_to_relocate);

      index_to_insert = get_alt_index(index_to_insert, fp_to_insert);
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
  std::tie(index, alt_index, fingerprint) = get_indexes_and_fingerprint_for(item);

  assert(alt_index == get_alt_index(index, fingerprint));
  assert(index == get_alt_index(alt_index, fingerprint));

  bool contained = m_buckets[index].contains(fingerprint)
                   || m_buckets[alt_index].contains(fingerprint);
  return contained;
}

template <typename T>
inline bool CuckooFilter<T>::erase(const T item) {
  size_t index;
  size_t alt_index;
  std::vector<uint8_t> fingerprint;
  std::tie(index, alt_index, fingerprint) = get_indexes_and_fingerprint_for(item);

  // TODO: Same element removed two times?
  bool erased = m_buckets[index].erase(fingerprint)
                || m_buckets[alt_index].erase(fingerprint);
  if (erased) {
    m_size--;
  }
  return erased;
}

template <typename T>
inline void CuckooFilter<T>::clear() {
  std::for_each(m_buckets.begin(), m_buckets.end(),
                [](Bucket& bucket) { bucket.clear(); });
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
  // (sizeof actually takes array-like padding into account)
  // assert that all Buckets have the same memory usage:
  size_t reserved_bucket_size = sizeof(Bucket);
  size_t used_bucket_size = m_buckets[0].memory_usage();
  return sizeof(CuckooFilter<T>) + used_bucket_size * m_buckets.size()
         + reserved_bucket_size * (m_buckets.capacity() - m_buckets.size());
}

template <typename T>
inline void CuckooFilter<T>::memory_usage_info() const {
  std::cerr << "== Cuckoofilter memory usage broken up: ==" << std::endl;
  std::cerr << "sizeof CuckooFilter struct: " << sizeof(CuckooFilter<T>) << "B"
            << std::endl;
  std::cerr << "number of used buckets: " << m_buckets.size() << std::endl;
  std::cerr << "number of excess buckets: "
            << m_buckets.capacity() - m_buckets.size() << std::endl;
  std::cerr << "single used bucket memory usage: "
            << m_buckets[0].memory_usage() << "B" << std::endl;
  std::cerr << "single unused bucket memory usage: " << sizeof(Bucket) << "B"
            << std::endl;
  m_buckets[0].memory_usage_info();
  std::cerr << "==========================================" << std::endl;
}

template <typename T>
inline std::ostream& operator<<(std::ostream& out,
                                const CuckooFilter<T>& filter) {
  out << "{" << std::endl;
  for (const auto& bucket : filter.m_buckets) {
    out << "  " << bucket << std::endl;
  }
  out << "}" << std::endl;
  return out;
}

} // namespace cuculiform
