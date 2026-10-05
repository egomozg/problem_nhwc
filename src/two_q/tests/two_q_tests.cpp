#include "2q.hpp"

#include <catch2/catch_test_macros.hpp>
#include <string>

namespace {
int load_page(int key) { return key; }
}

TEST_CASE("2Q: promotion and main queue hits do not reload", "[2q]") {
  cache_t<int> cache(4);
  int loads = 0;
  auto load = [&loads](int key) { ++loads; return key; };
  REQUIRE_FALSE(cache.full());
  REQUIRE_FALSE(cache.lookup_update(1, load));
  REQUIRE(cache.lookup_update(1, load)); // A1 -> Am.
  REQUIRE(cache.lookup_update(1, load)); // Am hit.
  REQUIRE(loads == 1);
  REQUIRE_FALSE(cache.full());
  for (int key : {2, 3, 4}) {
    REQUIRE_FALSE(cache.lookup_update(key, load));
  }
  REQUIRE(cache.full());
}

TEST_CASE("2Q: zero capacity skips the loader", "[2q]") {
  cache_t<int> cache(0);
  int loads = 0;
  auto load = [&loads](int key) { ++loads; return key; };
  REQUIRE(cache.full());
  REQUIRE_FALSE(cache.lookup_update(1, load));
  REQUIRE_FALSE(cache.lookup_update(1, load));
  REQUIRE(loads == 0);
}

TEST_CASE("2Q: capacity one evicts from either queue", "[2q]") {
  cache_t<int> cache(1);
  REQUIRE_FALSE(cache.lookup_update(1, load_page));
  SECTION("Am is empty and A1 equals the threshold") {
    REQUIRE_FALSE(cache.lookup_update(2, load_page));
    REQUIRE(cache.lookup_update(2, load_page));
    REQUIRE_FALSE(cache.lookup_update(1, load_page));
  }
  SECTION("A1 is empty after promotion") {
    REQUIRE(cache.lookup_update(1, load_page));
    REQUIRE_FALSE(cache.lookup_update(2, load_page));
    REQUIRE(cache.lookup_update(2, load_page));
    REQUIRE_FALSE(cache.lookup_update(1, load_page));
  }
}

TEST_CASE("2Q: new pages can exceed the threshold while space remains", "[2q]") {
  cache_t<int> cache(4); // Threshold 1, but all four pages fit in A1.
  for (int key : {1, 2, 3, 4}) {
    REQUIRE_FALSE(cache.lookup_update(key, load_page));
  }
  REQUIRE(cache.full());
  for (int key : {1, 2, 3, 4}) {
    REQUIRE(cache.lookup_update(key, load_page));
  }
}

TEST_CASE("2Q: A1 evicts the oldest arrival", "[2q]") {
  cache_t<int> cache(4);
  for (int key : {1, 2, 3, 4, 5}) {
    REQUIRE_FALSE(cache.lookup_update(key, load_page));
  }
  for (int key : {2, 3, 4, 5}) {
    REQUIRE(cache.lookup_update(key, load_page));
  }
  REQUIRE_FALSE(cache.lookup_update(1, load_page));
}

TEST_CASE("2Q: Am hits refresh the eviction order when A1 is empty", "[2q]") {
  cache_t<int> cache(2);
  for (int key : {1, 2}) {
    REQUIRE_FALSE(cache.lookup_update(key, load_page));
    REQUIRE(cache.lookup_update(key, load_page));
  }
  REQUIRE(cache.lookup_update(1, load_page));
  REQUIRE_FALSE(cache.lookup_update(3, load_page));
  REQUIRE(cache.lookup_update(1, load_page));
  REQUIRE_FALSE(cache.lookup_update(2, load_page));
}

TEST_CASE("2Q: equality with the threshold evicts from Am", "[2q]") {
  cache_t<int> cache(4); // Threshold 1.
  for (int key : {1, 2, 3}) {
    REQUIRE_FALSE(cache.lookup_update(key, load_page));
    REQUIRE(cache.lookup_update(key, load_page));
  }
  REQUIRE_FALSE(cache.lookup_update(4, load_page)); // A1 size == 1.
  REQUIRE_FALSE(cache.lookup_update(5, load_page)); // Evicts 1 from Am.
  REQUIRE(cache.lookup_update(4, load_page));
  REQUIRE(cache.lookup_update(5, load_page));
  REQUIRE_FALSE(cache.lookup_update(1, load_page));
}

TEST_CASE("2Q: a scan above the threshold preserves promoted pages", "[2q]") {
  cache_t<int> cache(4);
  REQUIRE_FALSE(cache.lookup_update(1, load_page));
  REQUIRE(cache.lookup_update(1, load_page));
  for (int key = 2; key <= 20; ++key) {
    REQUIRE_FALSE(cache.lookup_update(key, load_page));
  }
  REQUIRE(cache.lookup_update(1, load_page));
  REQUIRE_FALSE(cache.lookup_update(2, load_page));
}

TEST_CASE("2Q: threshold scales with capacity", "[2q]") {
  cache_t<int> cache(8); // Threshold 2.
  for (int key = 1; key <= 6; ++key) {
    REQUIRE_FALSE(cache.lookup_update(key, load_page));
    REQUIRE(cache.lookup_update(key, load_page));
  }
  for (int key : {7, 8, 9}) {
    REQUIRE_FALSE(cache.lookup_update(key, load_page));
  }
  REQUIRE(cache.lookup_update(7, load_page)); // Equality evicted Am's 1.
  REQUIRE_FALSE(cache.lookup_update(1, load_page));
}

TEST_CASE("2Q: keys need not equal values or have the same type", "[2q]") {
  cache_t<int, std::string> cache(1);
  int loads = 0;
  auto load = [&loads](const std::string&) { ++loads; return 42; };
  REQUIRE_FALSE(cache.lookup_update("one", load));
  REQUIRE(cache.lookup_update("one", load));
  REQUIRE_FALSE(cache.lookup_update("two", load));
  REQUIRE(cache.lookup_update("two", load));
  REQUIRE_FALSE(cache.lookup_update("one", load));
  REQUIRE(loads == 3);
}
