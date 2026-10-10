#include "2q.hpp"

#include <catch2/catch_test_macros.hpp>
#include <string>

namespace {
int load_page(int key) { return key; }

// For capacity 4: A1in = [5], Am = [3, 2, 1], A1out = [4].
// Lists in comments are ordered newest to oldest.
void prepare_main_queue(cache_t<int>& cache) {
  for (int key : {1, 2, 3, 4, 5}) {
    REQUIRE_FALSE(cache.lookup_update(key, load_page));
  }
  for (int key : {1, 2, 3}) {
    REQUIRE_FALSE(cache.lookup_update(key, load_page)); // Ghost misses promote.
  }
}
}

TEST_CASE("Full 2Q: zero capacity skips the loader", "[2q][full]") {
  cache_t<int> cache(0);
  int loads = 0;
  auto load = [&loads](int key) { ++loads; return key; };
  REQUIRE(cache.full());
  REQUIRE_FALSE(cache.lookup_update(1, load));
  REQUIRE_FALSE(cache.lookup_update(1, load));
  REQUIRE(loads == 0);
}

TEST_CASE("Full 2Q: free capacity is available to A1in above Kin", "[2q][full]") {
  cache_t<int> cache(4); // Kin = 1.
  for (int key : {1, 2, 3, 4}) {
    REQUIRE_FALSE(cache.full());
    REQUIRE_FALSE(cache.lookup_update(key, load_page));
  }
  REQUIRE(cache.full());
  for (int key : {1, 2, 3, 4}) {
    REQUIRE(cache.lookup_update(key, load_page));
  }
}

TEST_CASE("Full 2Q: A1in hits neither promote nor refresh FIFO order", "[2q][full]") {
  cache_t<int> cache(4);
  int loads = 0;
  auto load = [&loads](int key) { ++loads; return key; };
  for (int key : {1, 2, 3, 4}) {
    REQUIRE_FALSE(cache.lookup_update(key, load));
  }
  REQUIRE(cache.lookup_update(1, load)); // Oldest page stays oldest in A1in.
  REQUIRE(loads == 4);
  REQUIRE_FALSE(cache.lookup_update(5, load));
  for (int key : {2, 3, 4, 5}) {
    REQUIRE(cache.lookup_update(key, load));
  }
  REQUIRE_FALSE(cache.lookup_update(1, load)); // Evicted despite the hit.
  REQUIRE(loads == 6);
}

TEST_CASE("Full 2Q: ghost access reloads and promotes into Am", "[2q][full]") {
  cache_t<int> cache(4);
  int loads = 0;
  auto load = [&loads](int key) { ++loads; return key; };
  for (int key : {1, 2, 3, 4, 5}) {
    REQUIRE_FALSE(cache.lookup_update(key, load));
  }
  REQUIRE(loads == 5);
  REQUIRE_FALSE(cache.lookup_update(1, load)); // A1out holds only the key.
  REQUIRE(loads == 6);
  REQUIRE(cache.lookup_update(1, load)); // Now a resident Am hit.
  REQUIRE(loads == 6);
  for (int key = 6; key <= 20; ++key) {
    REQUIRE_FALSE(cache.lookup_update(key, load));
  }
  REQUIRE(cache.lookup_update(1, load)); // Promotion protects it from the scan.
  REQUIRE(loads == 21);
  REQUIRE_FALSE(cache.lookup_update(2, load));
}

TEST_CASE("Full 2Q: above Kin evicts from A1in with Am occupied", "[2q][full]") {
  cache_t<int> cache(4);
  for (int key : {1, 2, 3, 4, 5, 1, 2}) {
    REQUIRE_FALSE(cache.lookup_update(key, load_page));
  }
  // A1in = [5, 4], Am = [2, 1], A1out = [3]; Kin = 1.
  SECTION("A new key reclaims from A1in") {
    REQUIRE_FALSE(cache.lookup_update(6, load_page));
    REQUIRE(cache.lookup_update(1, load_page));
    REQUIRE(cache.lookup_update(2, load_page));
    REQUIRE(cache.lookup_update(5, load_page));
    REQUIRE_FALSE(cache.lookup_update(4, load_page));
  }
  SECTION("A ghost key also reclaims from A1in") {
    REQUIRE_FALSE(cache.lookup_update(3, load_page));
    REQUIRE(cache.lookup_update(1, load_page));
    REQUIRE(cache.lookup_update(2, load_page));
    REQUIRE(cache.lookup_update(3, load_page));
    REQUIRE(cache.lookup_update(5, load_page));
    REQUIRE_FALSE(cache.lookup_update(4, load_page));
  }
}

TEST_CASE("Full 2Q: equality with Kin evicts from Am", "[2q][full]") {
  cache_t<int> cache(4);
  prepare_main_queue(cache);
  SECTION("A new key reclaims from Am") {
    REQUIRE_FALSE(cache.lookup_update(6, load_page));
  }
  SECTION("A ghost key reclaims from Am") {
    REQUIRE_FALSE(cache.lookup_update(4, load_page));
    REQUIRE(cache.lookup_update(4, load_page));
  }
  REQUIRE(cache.lookup_update(5, load_page)); // A1in survives at the threshold.
  REQUIRE(cache.lookup_update(2, load_page));
  REQUIRE(cache.lookup_update(3, load_page));
  REQUIRE_FALSE(cache.lookup_update(1, load_page)); // Oldest Am page is gone.
}

TEST_CASE("Full 2Q: Am hits refresh LRU order without reloading", "[2q][full]") {
  cache_t<int> cache(4);
  prepare_main_queue(cache);
  int loads = 0;
  auto load = [&loads](int key) { ++loads; return key; };
  REQUIRE(cache.lookup_update(1, load)); // Am = [1, 3, 2].
  REQUIRE(loads == 0);
  REQUIRE_FALSE(cache.lookup_update(4, load)); // Ghost promotion evicts 2.
  REQUIRE(loads == 1);
  REQUIRE(cache.lookup_update(1, load));
  REQUIRE(cache.lookup_update(3, load));
  REQUIRE(cache.lookup_update(4, load));
  REQUIRE(loads == 1);
  REQUIRE_FALSE(cache.lookup_update(2, load));
  REQUIRE(loads == 2);
}

TEST_CASE("Full 2Q: Kout drops the oldest ghost but retains recent ones", "[2q][full]") {
  cache_t<int> cache(4); // Kout = 2.
  for (int key : {1, 2, 3, 4, 5, 6, 7}) {
    REQUIRE_FALSE(cache.lookup_update(key, load_page));
  }
  // A1out = [3, 2]; key 1 has been forgotten.
  SECTION("Forgotten key returns to A1in and is swept out") {
    REQUIRE_FALSE(cache.lookup_update(1, load_page));
    for (int key : {8, 9, 10, 11}) {
      REQUIRE_FALSE(cache.lookup_update(key, load_page));
    }
    REQUIRE_FALSE(cache.lookup_update(1, load_page));
  }
  SECTION("Oldest retained ghost is promoted and survives a scan") {
    REQUIRE_FALSE(cache.lookup_update(2, load_page));
    for (int key = 8; key <= 20; ++key) {
      REQUIRE_FALSE(cache.lookup_update(key, load_page));
    }
    REQUIRE(cache.lookup_update(2, load_page));
  }
}

TEST_CASE("Full 2Q: consuming a ghost frees a history slot", "[2q][full]") {
  cache_t<int> cache(4);
  for (int key : {1, 2, 3, 4, 5, 6}) {
    REQUIRE_FALSE(cache.lookup_update(key, load_page));
  }
  // Ghosts [2, 1]. Consuming 2 and evicting 3 must leave [3, 1].
  REQUIRE_FALSE(cache.lookup_update(2, load_page));
  REQUIRE_FALSE(cache.lookup_update(1, load_page));
  for (int key = 7; key <= 20; ++key) {
    REQUIRE_FALSE(cache.lookup_update(key, load_page));
  }
  REQUIRE(cache.lookup_update(1, load_page));
  REQUIRE(cache.lookup_update(2, load_page));
}

TEST_CASE("Full 2Q: evicted Am pages do not enter ghost history", "[2q][full]") {
  cache_t<int> cache(4);
  prepare_main_queue(cache);
  REQUIRE_FALSE(cache.lookup_update(6, load_page)); // Evicts 1 from Am.
  REQUIRE_FALSE(cache.lookup_update(1, load_page)); // Must enter A1in.
  for (int key = 7; key <= 12; ++key) {
    REQUIRE_FALSE(cache.lookup_update(key, load_page));
  }
  REQUIRE(cache.lookup_update(2, load_page));
  REQUIRE(cache.lookup_update(3, load_page));
  REQUIRE_FALSE(cache.lookup_update(1, load_page)); // No false promotion.
}

TEST_CASE("Full 2Q: capacity one handles empty Am and empty A1in", "[2q][full]") {
  cache_t<int> cache(1);
  REQUIRE_FALSE(cache.lookup_update(1, load_page));
  REQUIRE(cache.lookup_update(1, load_page)); // Still in A1in.
  REQUIRE_FALSE(cache.lookup_update(2, load_page)); // Am empty: evict A1in.
  REQUIRE_FALSE(cache.lookup_update(1, load_page)); // Ghost -> Am; A1in empty.
  REQUIRE(cache.lookup_update(1, load_page));
  SECTION("Cold miss evicts the only Am page") {
    REQUIRE_FALSE(cache.lookup_update(3, load_page));
    REQUIRE(cache.lookup_update(3, load_page));
    REQUIRE_FALSE(cache.lookup_update(1, load_page));
  }
  SECTION("Ghost miss evicts the only Am page") {
    REQUIRE_FALSE(cache.lookup_update(2, load_page));
    REQUIRE(cache.lookup_update(2, load_page));
    REQUIRE_FALSE(cache.lookup_update(1, load_page));
  }
}

TEST_CASE("Full 2Q: Kin scales with capacity", "[2q][full]") {
  cache_t<int> cache(8); // Kin = 2.
  for (int key = 1; key <= 9; ++key) {
    REQUIRE_FALSE(cache.lookup_update(key, load_page));
  }
  for (int key = 1; key <= 6; ++key) {
    REQUIRE_FALSE(cache.lookup_update(key, load_page)); // Ghost chain -> Am.
  }
  // A1in = [9, 8], Am = [6, 5, 4, 3, 2, 1], A1out = [7].
  REQUIRE_FALSE(cache.lookup_update(10, load_page)); // Equality: evict Am's 1.
  REQUIRE(cache.lookup_update(8, load_page));
  REQUIRE(cache.lookup_update(9, load_page));
  REQUIRE_FALSE(cache.lookup_update(1, load_page)); // Above Kin: evict A1in's 8.
  REQUIRE(cache.lookup_update(2, load_page));
  REQUIRE_FALSE(cache.lookup_update(8, load_page));
}

TEST_CASE("Full 2Q: Kout scales with capacity", "[2q][full]") {
  cache_t<int> cache(8); // Kout = 4.
  for (int key = 1; key <= 13; ++key) {
    REQUIRE_FALSE(cache.lookup_update(key, load_page));
  }
  // Ghosts [5, 4, 3, 2]; 1 is forgotten.
  SECTION("Four eviction records are retained") {
    REQUIRE_FALSE(cache.lookup_update(2, load_page));
    for (int key = 14; key <= 30; ++key) {
      REQUIRE_FALSE(cache.lookup_update(key, load_page));
    }
    REQUIRE(cache.lookup_update(2, load_page));
  }
  SECTION("The fifth eviction record is discarded") {
    REQUIRE_FALSE(cache.lookup_update(1, load_page));
    for (int key = 14; key <= 30; ++key) {
      REQUIRE_FALSE(cache.lookup_update(key, load_page));
    }
    REQUIRE_FALSE(cache.lookup_update(1, load_page));
  }
}

TEST_CASE("Full 2Q: ghost keys are independent of loaded values", "[2q][full]") {
  cache_t<int, std::string> cache(1);
  int loads = 0;
  auto load = [&loads](const std::string&) { ++loads; return 42; };
  REQUIRE_FALSE(cache.lookup_update("one", load));
  REQUIRE(cache.lookup_update("one", load));
  REQUIRE_FALSE(cache.lookup_update("two", load));
  REQUIRE_FALSE(cache.lookup_update("one", load)); // Ghost -> Am.
  REQUIRE(cache.lookup_update("one", load));
  REQUIRE(loads == 3);
  REQUIRE_FALSE(cache.lookup_update("two", load)); // Reclaims from Am.
  REQUIRE(cache.lookup_update("two", load));
  REQUIRE(loads == 4);
}
