#pragma once
#include "highwayhash/highwayhash.h"
#include <assert.h>

using namespace highwayhash;

namespace cuculiform {

// Change the implementation of this function signature to test other hash
// functions, e.g. SipHasher included in the highwayhash library.
// TODO: Make it easy to swap in a choice from several implementations, e.g. via
// templates
uint64_t strong_hash_fn(size_t value) {
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

template <typename T>
std::tuple<size_t, size_t, std::vector<uint8_t>>
indexes_and_fingerprint_for(const T item, const size_t fingerprint_size) {
  // use std::hash to normalize any type to a size_t.
  // Note that it doesn't necessarily produce distributed hashes,
  // i.e. for uints, it might just be the identity function.
  // To have an equally distributed hash, we then apply a strong hash function.
  std::hash<T> weak_hash_fn;
  uint64_t hash = strong_hash_fn(weak_hash_fn(item));
  if (hash == 0) {
    hash = 1;
    std::cerr
      << "Hash Collision with 0. This is probably nothing to worry about."
      << std::endl;
  }

  // split hash to lower and upper half
  const uint32_t fingerprint_part = static_cast<uint32_t>(hash);
  const uint32_t index_part = static_cast<uint32_t>(hash >> 32);

  // only use fingerprint_size bytes of the hash
  const uint32_t fingerprint = fingerprint_part >> 32 - fingerprint_size * 8;
  std::vector<uint8_t> fingerprint_vec(fingerprint_size);
  for (size_t i = 0; i < fingerprint_size; i++) {
    fingerprint_vec[i] = static_cast<uint8_t>((fingerprint_part >> i * 8) & 0xFF);
  }

  size_t index = index_part;
  size_t alt_index = index ^ static_cast<uint32_t>(strong_hash_fn(fingerprint));
  assert(index == alt_index
         ^ static_cast<uint32_t>(strong_hash_fn(fingerprint)));

  return std::make_tuple(index, alt_index, fingerprint_vec);
}

struct Bucket {
  explicit Bucket(size_t bucket_size, size_t fingerprint_size) {
    auto empty_fingerprint = std::vector<uint8_t>(fingerprint_size, 0);
    m_fingerprints =
      std::vector<std::vector<uint8_t>>(bucket_size, empty_fingerprint);
  }

  std::vector<std::vector<uint8_t>> m_fingerprints;

  bool insert(const std::vector<uint8_t> fingerprint);
  bool contains(const std::vector<uint8_t> fingerprint) const;
  bool erase(const std::vector<uint8_t> fingerprint);
  void clear();
};

bool Bucket::insert(const std::vector<uint8_t> fingerprint) {
  auto empty_fingerprint = std::vector<uint8_t>(fingerprint.size(), 0);
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

bool Bucket::contains(const std::vector<uint8_t> fingerprint) const {
  auto position =
    std::find(m_fingerprints.begin(), m_fingerprints.end(), fingerprint);
  return position != m_fingerprints.end();
}

bool Bucket::erase(const std::vector<uint8_t> fingerprint) {
  auto position =
    std::find(m_fingerprints.begin(), m_fingerprints.end(), fingerprint);
  bool has_fingerprint = position != m_fingerprints.end();
  if (has_fingerprint) {
    // found that fingerprint, delete it by filling its slot with zeros
    std::fill(position->begin(), position->end(), 0);
  }
  return has_fingerprint;
}

void Bucket::clear() {
  std::for_each(m_fingerprints.begin(), m_fingerprints.end(),
                [](std::vector<uint8_t>& fingerprint) {
                  std::fill(fingerprint.begin(), fingerprint.end(), 0);
                });
}

template <typename T>
class CuckooFilter {
public:
  explicit CuckooFilter(size_t capacity)
      : m_size(0),
        m_capacity(capacity),
        m_bucket_size(4),
        m_fingerprint_size(2) {
    assert(m_fingerprint_size > 0);
    assert(m_fingerprint_size <= 4);
    // round up to get required number of buckets
    size_t num_buckets = (m_capacity + m_bucket_size - 1) / m_bucket_size;
    Bucket empty_bucket{m_bucket_size, m_fingerprint_size};
    m_buckets = std::vector<Bucket>(num_buckets, empty_bucket);
  }

  bool insert(const T item);
  bool contains(const T item) const;
  bool erase(const T item);
  void clear();
  size_t size() const;
  size_t capacity() const;

private:
  size_t m_size;
  std::vector<Bucket> m_buckets;
  const size_t m_capacity;
  const size_t m_bucket_size;
  const size_t m_fingerprint_size;
};

template <typename T>
bool CuckooFilter<T>::insert(const T item) {
  size_t index;
  size_t alt_index;
  std::vector<uint8_t> fingerprint;
  std::tie(index, alt_index, fingerprint) =
    indexes_and_fingerprint_for(item, m_fingerprint_size);

  // TODO: rebucketing
  bool inserted =
    m_buckets[index % m_buckets.size()].insert(fingerprint)
    || m_buckets[alt_index % m_buckets.size()].insert(fingerprint);
  if (inserted) {
    m_size++;
  }
  return inserted;
}

template <typename T>
bool CuckooFilter<T>::contains(const T item) const {
  size_t index;
  size_t alt_index;
  std::vector<uint8_t> fingerprint;
  std::tie(index, alt_index, fingerprint) =
    indexes_and_fingerprint_for(item, m_fingerprint_size);

  bool contained =
    m_buckets[index % m_buckets.size()].contains(fingerprint)
    || m_buckets[alt_index % m_buckets.size()].contains(fingerprint);
  return contained;
}

template <typename T>
bool CuckooFilter<T>::erase(const T item) {
  size_t index;
  size_t alt_index;
  std::vector<uint8_t> fingerprint;
  std::tie(index, alt_index, fingerprint) =
    indexes_and_fingerprint_for(item, m_fingerprint_size);

  bool erased = m_buckets[index % m_buckets.size()].erase(fingerprint)
                || m_buckets[alt_index % m_buckets.size()].erase(fingerprint);
  if (erased) {
    m_size--;
  }
  return erased;
}

template <typename T>
void CuckooFilter<T>::clear() {
  std::for_each(m_buckets.begin(), m_buckets.end(),
                [](Bucket& bucket) { bucket.clear(); });
  m_size = 0;
}

template <typename T>
size_t CuckooFilter<T>::size() const {
  return m_size;
}

template <typename T>
size_t CuckooFilter<T>::capacity() const {
  return m_capacity;
}

} // namespace cuculiform
