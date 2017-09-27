#pragma once
#include <assert.h>

namespace cuculiform {

struct Bucket {
  std::vector<uint8_t> data;
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
    Bucket null_bucket;
    null_bucket.data =
      std::vector<uint8_t>(m_fingerprint_size * m_bucket_size, 0);
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

  // TODO: look in alternative bucket
  // TODO: rebucketing
  auto& bucket = m_buckets[index % m_buckets.size()];
  for (auto it = bucket.data.begin(); it != bucket.data.end();
       std::advance(it, m_fingerprint_size)) {
    auto end{it};
    std::advance(end, m_fingerprint_size);
    if (std::all_of(it, end, [](uint8_t i) { return i == 0; })) {
      // found an empty spot for a fingerprint. insert!
      std::copy(fingerprint_hash_bytes.begin(), fingerprint_hash_bytes.end(),
                it);
      m_size++;
      return true;
    }
  }

  return false;
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

  // TODO: look in alternative bucket
  auto& bucket = m_buckets[index % m_buckets.size()];
  for (auto it = bucket.data.begin(); it != bucket.data.end();
       std::advance(it, m_fingerprint_size)) {
    bool match = std::equal(fingerprint_hash_bytes.begin(),
                            fingerprint_hash_bytes.end(), it);
    if (match) {
      return true;
    }
  }

  return false;
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

  // TODO: look in alternative bucket
  auto& bucket = m_buckets[index % m_buckets.size()];
  for (auto it = bucket.data.begin(); it != bucket.data.end();
       std::advance(it, m_fingerprint_size)) {
    // iterate through the m_bucket_size fingerprints in this vector,
    // check whether they are == item, and if yes, set to 0.
    bool match = std::equal(fingerprint_hash_bytes.begin(),
                            fingerprint_hash_bytes.end(), it);
    if (match) {
      // found that fingerprint, delete it.
      auto end{it};
      std::advance(end, m_fingerprint_size);
      std::fill(it, end, 0);

      m_size--;
      return true;
    }
  }

  return false;
}

void CuckooFilter::clear() {
  for (auto& bucket : m_buckets) {
    std::fill(bucket.data.begin(), bucket.data.end(), 0);
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
