#pragma once

#include <algorithm>
#include <assert.h>
#include <functional>
#include <random>
#include <vector>

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

template <typename T>
class CuckooFilter {
public:
  explicit CuckooFilter(
    size_t capacity, size_t fingerprint_size, uint max_relocations = 10,
    std::function<uint64_t(size_t)> strong_hash_fn = highwayhash)
      : m_size(0),
        m_capacity(capacity),
        m_bucket_size(4),
        // for partial hashing to work, i.e. not generate invalid bucket
        // indexes.
        m_bucket_count(ceil_to_power_of_two(m_capacity / m_bucket_size)),
        m_fingerprint_size(fingerprint_size),
        m_max_relocations(max_relocations),
        m_strong_hash_fn(strong_hash_fn),
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
  const size_t m_capacity;
  const size_t m_bucket_size;
  const size_t m_bucket_count;
  const size_t m_fingerprint_size;
  const uint m_max_relocations;
  const std::function<uint64_t(size_t)> m_strong_hash_fn;
  std::mt19937 gen; // Standard mersenne_twister_engine seeded with rd()
  std::uniform_int_distribution<> index_dis;
  std::uniform_int_distribution<> bucket_dis;

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
                               const Fingerprint fingerprint) const {
  uint32_t fingerprint_linear = from_bytes(fingerprint);

  size_t alt_index =
    index
    ^ (static_cast<uint32_t>(m_strong_hash_fn(fingerprint_linear))
       % m_bucket_count);

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
  size_t index = index_part % m_bucket_count;
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
  std::tie(index, alt_index, fingerprint) =
    get_indexes_and_fingerprint_for(item);

  assert(index == get_alt_index(alt_index, fingerprint));

  size_t index_to_insert = index_dis(gen) ? index : alt_index;
  bool inserted = get_bucket(index_to_insert).insert(fingerprint);
  if (inserted) {
    m_size++;
    return true;
  }

  // TODO: insert two times the same value?
  Fingerprint fp_to_insert = fingerprint;
  index_to_insert = get_alt_index(index_to_insert, fp_to_insert);
  for (uint i = 0; i < m_max_relocations; i++) {
    auto bucket = get_bucket(index_to_insert);
    bool inserted = bucket.insert(fp_to_insert);
    if (inserted) {
      m_size++;
      return true;
    } else {
      size_t which_fingerprint_to_relocate = bucket_dis(gen);
      bucket.swap(fp_to_insert, which_fingerprint_to_relocate);

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
  std::cerr << "== Cuckoofilter memory usage broken up: ==" << std::endl;
  std::cerr << "sizeof CuckooFilter struct: " << sizeof(CuckooFilter<T>) << "B"
            << std::endl;
  std::cerr << "number of buckets: " << m_bucket_count << std::endl;
  std::cerr << "single bucket memory usage: " << sizeof(uint8_t) * m_bucket_size
            << "B" << std::endl;
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
