#pragma once

#include <algorithm>
#include <functional>
#include <iterator>

namespace chunked_iterator {

template <class It>
class Chunk {
public:
  explicit Chunk(It begin, It end) : begin(begin), end(end) {
  }

  inline bool operator==(const Chunk& other) const {
    return std::equal(begin, end, other.begin);
  }
  // to make operator-> in ChunkedIterator work
  inline const Chunk<It>* operator->() const {
    return this;
  }

  It begin;
  It end;
};

// ChunkedIterator allows to iterate the given It iterator
// in Chunks of user-defined size
// The only good source to implement your own random access iterators I could
// find: Modern C++ Programming Cookbook by Marius Bancila, Page 230
template <class It>
class ChunkedIterator {
public:
  // implement std::iterator_traits
  using difference_type = ptrdiff_t;
  // Chunks are the values being worked on by e.g. std::search
  using value_type = Chunk<It>;
  // as well as the reference type, being a proxy to the underlying iterator
  using reference = Chunk<It>;
  // but not the pointer, as they can't be null or dereferenced, and as they
  // don't seem to be required for function signatures, there should no issues
  // with lifetime of a return value
  using pointer = Chunk<It>*;
  using iterator_category = std::random_access_iterator_tag;

  explicit ChunkedIterator(It underlying, size_t chunk_size, size_t index = 0)
      : underlying(underlying), index(index), chunk_size(chunk_size) {
  }

  // default constructor for forward iterator trait
  ChunkedIterator() = default;
  ChunkedIterator(const ChunkedIterator&) = default;
  ChunkedIterator& operator=(const ChunkedIterator&) = default;
  ~ChunkedIterator() = default;

  inline ChunkedIterator& operator++() {
    ++index;
    return *this;
  }
  inline ChunkedIterator operator++(int) {
    ChunkedIterator tmp(*this);
    ++*this;
    return tmp;
  }
  // implement input iterator trait
  inline bool operator==(const ChunkedIterator& other) const {
    assert(underlying == other.underlying);
    return index == other.index;
  }
  inline bool operator!=(const ChunkedIterator& other) const {
    return !(*this == other);
  }
  inline reference operator*() const {
    return reference(std::next(underlying, index * chunk_size),
                     std::next(underlying, (index + 1) * chunk_size));
  }
  inline reference operator->() const {
    return reference(std::next(underlying, index * chunk_size),
                     std::next(underlying, (index + 1) * chunk_size));
  }
  // implement bidirectional iterator trait
  inline ChunkedIterator& operator--() {
    assert(index > 0);
    --index;
    return *this;
  }
  inline ChunkedIterator operator--(int) {
    ChunkedIterator tmp(*this);
    --*this;
    return tmp;
  }
  // implement random access iterator trait
  inline ChunkedIterator operator+(difference_type offset) const {
    ChunkedIterator tmp(*this);
    return tmp += offset;
  }
  inline ChunkedIterator operator-(difference_type offset) const {
    ChunkedIterator tmp(*this);
    return tmp -= offset;
  }
  inline difference_type operator-(const ChunkedIterator& other) const {
    assert(underlying == other.underlying);
    return index - other.index;
  }
  inline bool operator<(const ChunkedIterator& other) const {
    assert(underlying == other.underlying);
    return index < other.index;
  }
  inline bool operator>(const ChunkedIterator& other) const {
    return other < *this;
  }
  inline bool operator<=(const ChunkedIterator& other) const {
    return !(other < *this);
  }
  inline bool operator>=(const ChunkedIterator& other) const {
    return !(*this < other);
  }
  inline ChunkedIterator& operator+=(const difference_type offset) {
    index += offset;
    return *this;
  }
  inline ChunkedIterator& operator-=(const difference_type offset) {
    return *this += -offset;
  }
  inline reference operator[](const difference_type offset) {
    return reference(std::next(underlying, ((index + offset) * chunk_size)),
                     std::next(underlying, (index + offset + 1) * chunk_size));
  }
  inline const reference operator[](const difference_type offset) const {
    return reference(std::next(underlying, ((index + offset) * chunk_size)),
                     std::next(underlying, (index + offset + 1) * chunk_size));
  }

private:
  It underlying;
  size_t index = 0;
  const size_t chunk_size = 0;
};

} // namespace chunked_iterator
