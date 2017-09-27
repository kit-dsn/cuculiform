#pragma once
#include <assert.h>

namespace cuculiform {

struct Bucket {
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
    // found empty position, insert
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
    // found that fingerprint, delete it.
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
    null_bucket.m_fingerprints =
      std::vector<std::vector<uint8_t>>(m_bucket_size, empty_fingerprint);
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

  std::vector<uint8_t> fingerprint(m_fingerprint_size);
  for (size_t i = 0; i < m_fingerprint_size; i++) {
    fingerprint[i] = static_cast<uint8_t>((lower_hash >> i * 8) & 0xFF);
  }

  size_t index = upper_hash;
  // TODO: alt_index = index ^ H(lower_hash) für eine Art H
  size_t alt_index = index ^ lower_hash;

  // TODO: rebucketing
  bool inserted =
    m_buckets[index % m_buckets.size()].insert(fingerprint)
    || m_buckets[alt_index % m_buckets.size()].insert(fingerprint);
  if (inserted) {
    m_size++;
  }
  return inserted;
}

bool CuckooFilter::contains(const uint64_t item) const {
  // H = id ;)
  const uint64_t hash = item;

  const uint32_t lower_hash = static_cast<uint32_t>(hash);
  const uint32_t upper_hash = static_cast<uint32_t>(hash >> 32);

  // TODO: devise method to avoid hashes that are == 0 to be able to separate
  // them from empty bucket slots
  assert(lower_hash != 0);

  std::vector<uint8_t> fingerprint(m_fingerprint_size);
  for (size_t i = 0; i < m_fingerprint_size; i++) {
    fingerprint[i] = static_cast<uint8_t>((lower_hash >> i * 8) & 0xFF);
  }

  size_t index = upper_hash;
  // TODO: alt_index = index ^ H(lower_hash) für eine Art H
  size_t alt_index = index ^ lower_hash;

  bool contained =
    m_buckets[index % m_buckets.size()].contains(fingerprint)
    || m_buckets[alt_index % m_buckets.size()].contains(fingerprint);
  return contained;
}

bool CuckooFilter::erase(const uint64_t item) {
  // H = id ;)
  const uint64_t hash = item;

  const uint32_t lower_hash = static_cast<uint32_t>(hash);
  const uint32_t upper_hash = static_cast<uint32_t>(hash >> 32);

  // TODO: devise method to avoid hashes that are ==0 to be able to separate
  // them from empty bucket slots
  assert(lower_hash != 0);

  std::vector<uint8_t> fingerprint(m_fingerprint_size);
  for (size_t i = 0; i < m_fingerprint_size; i++) {
    fingerprint[i] = static_cast<uint8_t>((lower_hash >> i * 8) & 0xFF);
  }

  size_t index = upper_hash;
  // TODO: alt_index = index ^ H(lower_hash) für eine Art H
  size_t alt_index = index ^ lower_hash;

  bool erased = m_buckets[index % m_buckets.size()].erase(fingerprint)
                || m_buckets[alt_index % m_buckets.size()].erase(fingerprint);
  if (erased) {
    m_size--;
  }
  return erased;
}

void CuckooFilter::clear() {
  std::for_each(m_buckets.begin(), m_buckets.end(),
                [](Bucket& bucket) { bucket.clear(); });
  m_size = 0;
}

size_t CuckooFilter::size() const {
  return m_size;
}

size_t CuckooFilter::capacity() const {
  return m_capacity;
}

} // namespace cuculiform
