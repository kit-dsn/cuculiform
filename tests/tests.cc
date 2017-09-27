#define CATCH_CONFIG_MAIN
#include "catch/catch.hpp"
#include "cuculiform.h"

TEST_CASE("create cuckoofilter", "[cuculiform]") {
  size_t capacity = 1024;
  cuculiform::CuckooFilter filter{capacity};

  REQUIRE(filter.size() == 0);
  REQUIRE(filter.capacity() == capacity);
  REQUIRE(filter.contains(1) == false);
  REQUIRE(filter.contains(2) == false);

  // chosen by a fair dice roll
  REQUIRE(filter.insert(4) == true);
  REQUIRE(filter.insert(8) == true);
  REQUIRE(filter.size() == 2);

  REQUIRE(filter.contains(4) == true);
  REQUIRE(filter.contains(5) == false);

  REQUIRE(filter.erase(4) == true);
  REQUIRE(filter.erase(5) == false);
  REQUIRE(filter.size() == 1);
  REQUIRE(filter.contains(4) == false);

  REQUIRE(filter.insert(5) == true);
  filter.clear();
  REQUIRE(filter.size() == 0);
  REQUIRE(filter.contains(5) == false);

  REQUIRE(filter.contains(0) == false);
  REQUIRE(filter.insert(0) == true);
  REQUIRE(filter.contains(0) == true);

  for (uint64_t i = 0; i < 100; i++) {
    REQUIRE(filter.insert(i) == true);
  }
}
