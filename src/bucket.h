#pragma once

#include <algorithm>
#include <assert.h>
#include <iterator>
#include <vector>

#include "chunked_iterator.h"

using namespace chunked_iterator;

namespace cuculiform {

class Bucket {
public:
  using iterator = ChunkedIterator<std::vector<uint8_t>::iterator>;
  using const_iterator = ChunkedIterator<std::vector<uint8_t>::const_iterator>;

  explicit Bucket(std::vector<uint8_t>::iterator begin,
                  std::vector<uint8_t>::iterator end, size_t fingerprint_size)
      : m_begin(begin), m_end(end), m_fingerprint_size(fingerprint_size) {
  }

  iterator begin();
  iterator end();
  const_iterator begin() const;
  const_iterator end() const;
  const_iterator cbegin() const;
  const_iterator cend() const;

  bool insert(const std::vector<uint8_t> fingerprint);
  void swap(std::vector<uint8_t>& fingerprint, size_t index);
  bool contains(const std::vector<uint8_t> fingerprint) const;
  bool erase(const std::vector<uint8_t> fingerprint);
  void clear();

private:
  // TODO: misleading name,
  // could be thought this is the same as begin() and end()
  std::vector<uint8_t>::iterator m_begin;
  std::vector<uint8_t>::iterator m_end;
  const size_t m_fingerprint_size;
};

inline Bucket::iterator Bucket::begin() {
  return iterator(m_begin, m_fingerprint_size, 0);
}
inline Bucket::iterator Bucket::end() {
  return iterator(m_begin, m_fingerprint_size,
                  std::distance(m_begin, m_end) / m_fingerprint_size);
}
inline Bucket::const_iterator Bucket::begin() const {
  return const_iterator(m_begin, m_fingerprint_size, 0);
}
inline Bucket::const_iterator Bucket::end() const {
  return const_iterator(m_begin, m_fingerprint_size,
                        std::distance(m_begin, m_end) / m_fingerprint_size);
}
inline Bucket::const_iterator Bucket::cbegin() const {
  return const_iterator(m_begin, m_fingerprint_size, 0);
}
inline Bucket::const_iterator Bucket::cend() const {
  return const_iterator(m_begin, m_fingerprint_size,
                        std::distance(m_begin, m_end));
}

inline bool Bucket::insert(const std::vector<uint8_t> fingerprint) {
  // TODO: can empty_fingerprint (and empty_chunk) be allocated once as static
  // member variable so that it is allocated only once? Problem is, we don't
  // know the m_fingerprint_size for a static member variable of Bucket, and
  // can't use template parameters because then we'd need dynamic dispatch to
  // the right bucket variant in CuckooFilter. :/
  auto empty_fingerprint = std::vector<uint8_t>(m_fingerprint_size, 0);
  assert(empty_fingerprint != fingerprint);
  auto empty_chunk =
    iterator::value_type(empty_fingerprint.begin(), empty_fingerprint.end());
  auto position = std::find(begin(), end(), empty_chunk);
  bool has_empty_position = position != end();
  if (has_empty_position) {
    // found empty position, insert by bytewise-copying the fingerprint into
    // that slot
    std::copy(fingerprint.begin(), fingerprint.end(), position->begin);
  }
  return has_empty_position;
}

inline void Bucket::swap(std::vector<uint8_t>& fingerprint, size_t index) {
  auto chunk = begin()[index];
  // NOTE: could be done with std::swap if Chunk were copy / move constructible
  // from argument
  std::swap_ranges(fingerprint.begin(), fingerprint.end(), chunk.begin);
}

inline bool Bucket::contains(const std::vector<uint8_t> fingerprint) const {
  // NOTE: is value_type semantically correct? Should it be ::reference instead?
  // (doesn't matter though, both is Chunk)
  auto chunk =
    const_iterator::value_type(fingerprint.begin(), fingerprint.end());
  auto position = std::find(cbegin(), cend(), chunk);
  return position != cend();
}

inline bool Bucket::erase(std::vector<uint8_t> fingerprint) {
  auto chunk = iterator::value_type(fingerprint.begin(), fingerprint.end());
  auto position = std::find(begin(), end(), chunk);
  bool has_fingerprint = position != end();
  if (has_fingerprint) {
    // found that fingerprint, delete it by filling its slot with zeros
    std::fill(position->begin, position->end, 0);
  }
  return has_fingerprint;
}

} // namespace cuculiform
