#pragma once
#include <assert.h>

namespace cuculiform {

struct Bucket {
  std::vector<std::vector<uint8_t>> fingerprints;
};

class CuckooFilter {
public:
  explicit CuckooFilter(size_t capacity)
      : m_size(0),
        m_capacity(capacity),
        m_bucket_size(4),
        m_fingerprint_size(2) {
    // round up to get required number of buckets
    size_t num_buckets = (m_capacity + m_bucket_size - 1) / m_bucket_size;
    auto empty_fingerprint = std::vector<uint8_t>(m_fingerprint_size, 0);
    Bucket null_bucket;
    null_bucket.fingerprints = std::vector<std::vector<uint8_t>>(m_bucket_size, empty_fingerprint);
    m_buckets = std::vector<Bucket>(num_buckets, null_bucket);
  }

  bool insert(const uint64_t item);
  bool contains(const uint64_t item) const;
  bool erase(const uint64_t item);
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

bool CuckooFilter::insert(const uint64_t item) {
  // H = id ;)
  const uint64_t hash = item;

  const uint32_t lower_hash = static_cast<uint32_t>(hash);
  const uint32_t upper_hash = static_cast<uint32_t>(hash >> 32);

  // TODO: devise method to avoid hashes that are ==0 to be able to separate
  // them from empty bucket slots
  assert(lower_hash != 0);

  std::vector<uint8_t> fingerprint_hash_bytes(m_fingerprint_size);
  for (size_t i = 0; i < m_fingerprint_size; i++) {
    fingerprint_hash_bytes[i] =
      static_cast<uint8_t>((lower_hash >> i * 8) & 0xFF);
  }

  size_t index = upper_hash;
  // TODO: alt_index = index ^ H(lower_hash) für eine Art H
  size_t alt_index = index ^ lower_hash;

  // TODO: rebucketing
  auto empty_fingerprint = std::vector<uint8_t>(m_fingerprint_size, 0);
  auto& bucket = m_buckets[index % m_buckets.size()];
  auto result = std::find(bucket.fingerprints.begin(), bucket.fingerprints.end(), empty_fingerprint);
  if (result != bucket.fingerprints.end()) {
    // found an empty spot for a fingerprint. insert!
    std::copy(fingerprint_hash_bytes.begin(), fingerprint_hash_bytes.end(),
              result->begin());
    m_size++;
    return true;
  }
  else {
    auto& bucket = m_buckets[alt_index % m_buckets.size()];
    auto result = std::find(bucket.fingerprints.begin(), bucket.fingerprints.end(), empty_fingerprint);
    if (result != bucket.fingerprints.end()) {
      // found an empty spot for a fingerprint. insert!
      std::copy(fingerprint_hash_bytes.begin(), fingerprint_hash_bytes.end(),
                result->begin());
      m_size++;
      return true;
    }
    else {
      return false;
    }
  }
}

bool CuckooFilter::contains(const uint64_t item) const {
  // H = id ;)
  const uint64_t hash = item;

  const uint32_t lower_hash = static_cast<uint32_t>(hash);
  const uint32_t upper_hash = static_cast<uint32_t>(hash >> 32);

  // TODO: devise method to avoid hashes that are == 0 to be able to separate
  // them from empty bucket slots
  assert(lower_hash != 0);

  std::vector<uint8_t> fingerprint_hash_bytes(m_fingerprint_size);
  for (size_t i = 0; i < m_fingerprint_size; i++) {
    fingerprint_hash_bytes[i] =
      static_cast<uint8_t>((lower_hash >> i * 8) & 0xFF);
  }

  size_t index = upper_hash;
  // TODO: alt_index = index ^ H(lower_hash) für eine Art H
  size_t alt_index = index ^ lower_hash;

  auto& bucket = m_buckets[index % m_buckets.size()];
  auto result = std::find(bucket.fingerprints.begin(), bucket.fingerprints.end(), fingerprint_hash_bytes);
  if (result != bucket.fingerprints.end()) {
    return true;
  }
  else {
    auto& bucket = m_buckets[alt_index % m_buckets.size()];
    auto result = std::find(bucket.fingerprints.begin(), bucket.fingerprints.end(), fingerprint_hash_bytes);
    return result != bucket.fingerprints.end();
  }
}

bool CuckooFilter::erase(const uint64_t item) {
  // H = id ;)
  const uint64_t hash = item;

  const uint32_t lower_hash = static_cast<uint32_t>(hash);
  const uint32_t upper_hash = static_cast<uint32_t>(hash >> 32);

  // TODO: devise method to avoid hashes that are ==0 to be able to separate
  // them from empty bucket slots
  assert(lower_hash != 0);

  std::vector<uint8_t> fingerprint_hash_bytes(m_fingerprint_size);
  for (size_t i = 0; i < m_fingerprint_size; i++) {
    fingerprint_hash_bytes[i] =
      static_cast<uint8_t>((lower_hash >> i * 8) & 0xFF);
  }

  size_t index = upper_hash;
  // TODO: alt_index = index ^ H(lower_hash) für eine Art H
  size_t alt_index = index ^ lower_hash;

  auto& bucket = m_buckets[index % m_buckets.size()];
  auto result = std::find(bucket.fingerprints.begin(), bucket.fingerprints.end(), fingerprint_hash_bytes);
  if (result != bucket.fingerprints.end()) {
    // found that fingerprint, delete it.
    std::fill(result->begin(), result->end(), 0);
    m_size--;
    return true;
  }
  else {
    auto& bucket = m_buckets[alt_index % m_buckets.size()];
    auto result = std::find(bucket.fingerprints.begin(), bucket.fingerprints.end(), fingerprint_hash_bytes);
    if (result != bucket.fingerprints.end()) {
      // found that fingerprint, delete it.
      std::fill(result->begin(), result->end(), 0);
      m_size--;
      return true;
    }
    else {
      return false;
    }
  }
}

void CuckooFilter::clear() {
  for (auto& bucket : m_buckets) {
    for (auto& fingerprint : bucket.fingerprints) {
      std::fill(fingerprint.begin(), fingerprint.end(), 0);
    }
  }
  m_size = 0;
}

size_t CuckooFilter::size() const {
  return m_size;
}

size_t CuckooFilter::capacity() const {
  return m_capacity;
}

} // namespace cuculiform
