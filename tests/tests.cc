#define CATCH_CONFIG_MAIN
#include "catch/catch.hpp"
#include "cuculiform.h"

#include <functional>
#include <string>

#include <iostream>
#include <locale>

#include <chrono>
#include <ctime>

TEST_CASE("create cuckoofilter", "[cuculiform]") {
  size_t capacity = 1024;
  size_t fingerprint_size = 2;
  cuculiform::CuckooFilter<uint64_t> filter{capacity, fingerprint_size};

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
  size_t fingerprint_size = 2;
  cuculiform::CuckooFilter<std::string> filter{capacity, fingerprint_size};
  REQUIRE(filter.insert("helloworld") == true);
  REQUIRE(filter.contains("helloworld") == true);
  REQUIRE(filter.contains("1337") == false);
  REQUIRE(filter.erase("helloworld") == true);
  REQUIRE(filter.contains("helloworld") == false);
}

TEST_CASE("false positive test", "[cuculiform]") {
  size_t capacity = 1 << 20;
  size_t fingerprint_size = 2;
  /* cuculiform::CuckooFilter<uint64_t> filter{capacity, fingerprint_size, 500,
   * cuculiform::TwoIndependentMultiplyShift{}}; */
  cuculiform::CuckooFilter<uint64_t> filter{capacity, fingerprint_size};

  size_t num_insertions = 0;
  bool failed_rebucketing = false;
  // We might not be able to get all items in, but still there should be enough
  // so we can just use what has fit in and continue with the test.
  for (size_t i = 0; i < capacity; i++) {
    num_insertions++;
    if (!filter.insert(i)) {
      // NOTE: Although insert returns false, the item is inserted but another
      //       one has been kicked out due to the relocation process.
      std::cerr << "could not relocate when inserting " << i << std::endl;
      failed_rebucketing = true;
      break;
    }
  }
  if (!failed_rebucketing) {
    std::cerr << "could insert all elements" << std::endl;
  }
  // The range [0..n-1,n+1..num_insertions) are all known to be in the filter,
  // where n is the item that was kicked out during max relocations.
  // In sum, num_insertions items should be contained in the filter, -1 if
  // failed_rebucketing
  size_t num_contained = 0;
  size_t missing_elements = 0;

  for (size_t i = 0; i < num_insertions; i++) {
    if (filter.contains(i)) {
      num_contained++;
    } else {
      missing_elements++;
      std::cout << "evicted element: " << i << std::endl;
    }
  }

  REQUIRE(missing_elements <= static_cast<size_t>(failed_rebucketing));
  REQUIRE(num_insertions - failed_rebucketing <= num_contained);

  // Everything above capacity is known *not* to be in the filter.
  // Every element for which the filter claims that it is contained is
  // therefore a false positive.
  size_t false_queries = 0;
  size_t queries = capacity;
  std::chrono::time_point<std::chrono::system_clock> start, end;
  start = std::chrono::system_clock::now();

  for (size_t i = capacity; i < capacity + queries; i++) {
    if (filter.contains(i)) {
      false_queries += 1;
    }
  }

  end = std::chrono::system_clock::now();
  int elapsed_time =
    std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  double time_per_contain = (static_cast<double>(elapsed_time) * 1000.0)
                            / static_cast<double>(queries);
  std::cout << "elapsed time: " << elapsed_time << "ms\n";
  std::cout << "time per contain operation: " << time_per_contain << "μs\n";

  double false_positive_rate =
    static_cast<double>(false_queries) / static_cast<double>(queries);

  // Use the user-preferred locale to get thousand separators in output
  std::cout.imbue(std::locale(""));

  // TODO: REQUIRE a minimum number or ratio of inserted events
  std::cout << "number of insertions: " << num_insertions << std::endl;
  std::cout << "elements contained:   " << num_contained << std::endl;
  // TODO: REQUIRE some memory usage value or range
  size_t memory_usage_KiB = filter.memory_usage() / 1024;
  double memory_usage_per_element = static_cast<double>(filter.memory_usage())
                                    / static_cast<double>(num_contained);
  std::cout << "memory usage: " << memory_usage_KiB << "KiB" << std::endl;
  std::cout << "lower bound on memory usage: "
            << (capacity * fingerprint_size) / 1024 << "KiB" << std::endl;
  std::cout << "memory usage per element: " << memory_usage_per_element << "B"
            << std::endl;
  std::cout << "lower bound on memory usage per element: " << fingerprint_size
            << "B" << std::endl;
  filter.memory_usage_info();
  std::cout << "filter is at "
            << (num_insertions - failed_rebucketing)
                 / static_cast<double>(capacity)
            << " of capacity" << std::endl;
  std::cout << "false positive rate: " << false_positive_rate << "%"
            << std::endl;

  // ratio should be around 0.024, round up to 0.03
  // to accomodate for random fluctuation
  REQUIRE(false_positive_rate < 0.03);
}
