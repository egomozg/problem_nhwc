#include "lru.hpp"

#include <catch2/catch_test_macros.hpp>
#include <string>

namespace {
int load_page(int key) { return key; }
}

TEST_CASE("LRU: A hit does not load the page again", "[lru]") {
  cache_t<int> cache(2);
  int loads = 0;
  auto load = [&loads](int key) {
    ++loads;
    return key;
  };

  REQUIRE_FALSE(cache.full());
  REQUIRE_FALSE(cache.lookup_update(10, load));
  REQUIRE(cache.lookup_update(10, load));
  REQUIRE(loads == 1);
  REQUIRE_FALSE(cache.full());
  REQUIRE_FALSE(cache.lookup_update(20, load));
  REQUIRE(cache.full());
}

TEST_CASE("LRU: A hit protects the page from the next eviction", "[lru]") {
  cache_t<int> cache(2);
  REQUIRE_FALSE(cache.lookup_update(1, load_page));
  REQUIRE_FALSE(cache.lookup_update(2, load_page));
  REQUIRE(cache.lookup_update(1, load_page));
  REQUIRE_FALSE(cache.lookup_update(3, load_page));
  REQUIRE(cache.lookup_update(1, load_page));
  REQUIRE(cache.lookup_update(3, load_page));
  REQUIRE(cache.full());
  REQUIRE_FALSE(cache.lookup_update(2, load_page));
}

TEST_CASE("LRU: A cache of capacity one replaces its only page", "[lru]") {
  cache_t<int> cache(1);
  REQUIRE_FALSE(cache.lookup_update(1, load_page));
  REQUIRE(cache.full());
  REQUIRE(cache.lookup_update(1, load_page));
  REQUIRE_FALSE(cache.lookup_update(2, load_page));
  REQUIRE(cache.full());
  REQUIRE(cache.lookup_update(2, load_page));
  REQUIRE_FALSE(cache.lookup_update(1, load_page));
}

TEST_CASE("LRU: Zero capacity never caches or calls the loader", "[lru]") {
  cache_t<int> cache(0);
  int loads = 0;
  auto load = [&loads](int key) {
    ++loads;
    return key;
  };
  REQUIRE(cache.full());
  REQUIRE_FALSE(cache.lookup_update(1, load));
  REQUIRE_FALSE(cache.lookup_update(1, load));
  REQUIRE(loads == 0);
}

TEST_CASE("LRU: Keys and stored values can have different types", "[lru]") {
  cache_t<std::string> cache(1);
  auto load = [](int key) { return "page " + std::to_string(key); };
  REQUIRE_FALSE(cache.lookup_update(5, load));
  REQUIRE(cache.cache_.front().second == "page 5");
  REQUIRE(cache.lookup_update(5, load));
  REQUIRE_FALSE(cache.lookup_update(7, load));
  REQUIRE_FALSE(cache.lookup_update(5, load));
}

TEST_CASE("LRU: An integer value may not equal its key", "[lru]") {
  cache_t<int> cache(1);
  auto load = [](int key) { return key + 100; };
  REQUIRE_FALSE(cache.lookup_update(5, load));
  REQUIRE(cache.cache_.front().second == 105);
  REQUIRE(cache.cache_.front().first == 5);
  REQUIRE_FALSE(cache.lookup_update(7, load));
  REQUIRE(cache.cache_.front().second == 107);
  REQUIRE(cache.cache_.front().first == 7);
  REQUIRE_FALSE(cache.lookup_update(5, load));
}
