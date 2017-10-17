#define CATCH_CONFIG_MAIN
#include "catch/catch.hpp"
#include "cuculiform.h"

#include <functional>
#include <string>

#include <iostream>

TEST_CASE("create cuckoofilter", "[cuculiform]") {
  size_t capacity = 1024;
  cuculiform::CuckooFilter<uint64_t> filter{capacity};

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
}

TEST_CASE("string cuckoofilter", "[cuculiform]") {
  size_t capacity = 1024;
  cuculiform::CuckooFilter<std::string> filter{capacity};
  REQUIRE(filter.insert("helloworld") == true);
  REQUIRE(filter.contains("helloworld") == true);
  REQUIRE(filter.contains("1337") == false);
  REQUIRE(filter.erase("helloworld") == true);
  REQUIRE(filter.contains("helloworld") == false);
}

TEST_CASE("false positive test", "[cuculiform]") {
  size_t total_items = 1000000;
  cuculiform::CuckooFilter<uint64_t> filter{total_items};

  size_t num_inserted = 0;
  // We might not be able to get all items in, but still there should be enough
  // so we can just use what has fit in and continue with the test.
  for (size_t i = 0; i < total_items; i++) {
    if (!filter.insert(i)) {
      num_inserted = i;
      break;
    }
  }

  // The range 0..num_inserted are all known to be in the filter.
  // The filter shouldn't return false negatives, and therefore they should all
  // be contained.
  for (size_t i = 0; i < num_inserted; i++) {
    REQUIRE(filter.contains(i));
  }

  // The range total_items..(2 * total_items) are all known *not* to be in the
  // filter. Every element for which the filter claims that it is contained is
  // therefore a false positive.
  size_t false_queries = 0;
  for (size_t i = total_items; i < 2 * total_items; i++) {
    if (filter.contains(i)) {
      false_queries += 1;
    }
  }
  double false_positive_rate =
    static_cast<double>(false_queries) / static_cast<double>(total_items);

  // TODO: REQUIRE a minimum number or ratio of inserted events
  std::cout << "elements inserted: " << num_inserted << std::endl;
  // TODO: REQUIRE a memory usage
  std::cout << "false positive rate: " << false_positive_rate << "%"
            << std::endl;
  // ratio should be around 0.024, round up to 0.03 to accomodate for random
  // fluctuation
  REQUIRE(false_positive_rate < 0.03);
}
