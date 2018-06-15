#pragma once

#include <random>
#include <vector>

#include "highwayhash/highwayhash.h"
#include "city.h"

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

// implements the required hash function signature
// for CuckooFilter using HighwayHash
class HighwayHash {
public:
  HighwayHash() {}

  uint64_t operator()(size_t value) const {
    using namespace highwayhash;

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
};

// implements the required hash function signature
// for CuckooFilter using CityHash
class CityHash {
  uint64_t seed;

public:
  CityHash() : seed(0) {}
  CityHash(uint64_t seed) : seed(seed) {}

  uint64_t operator()(size_t value) const {
    char bytes[sizeof(value)];
    for (size_t i = 0; i < sizeof(value); i++) {
      bytes[i] = static_cast<uint8_t>((value >> i * 8) & 0xFF);
    }

    if(seed != 0){
      return CityHash64WithSeed(bytes, sizeof(bytes), seed);
    }else
      return CityHash64(bytes, sizeof(bytes));
  }
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
// TODO: almost copied from cuckoofilter, reimplement from scratch?
// TODO: is 128bit really needed? Check paper / other implementations.
// Taken from:
// https://github.com/efficient/cuckoofilter/blob/master/src/hashutil.h#L49
// See Martin Dietzfelbinger, "Universal hashing and k-wise independent random
// variables via integer arithmetic without primes".
class TwoIndependentMultiplyShift {
  unsigned __int128 multiply, add;

public:
  TwoIndependentMultiplyShift() {
    std::random_device random;
    for (auto v : {&multiply, &add}) {
      *v = random();
      for (int i = 0; i < 4; ++i) {
        *v = *v << 32;
        *v |= random();
      }
    }
  }

  uint64_t operator()(uint64_t value) const {
    return (add + multiply * static_cast<decltype(multiply)>(value)) >> 64;
  }
};
#pragma GCC diagnostic pop

} // namespace cuculiform
