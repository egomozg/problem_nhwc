#include "arc.hpp"

#include <catch2/catch_test_macros.hpp>
#include <string>

namespace {
    int load_page(int key) { return key; }
}

TEST_CASE("ARC: Zero capacity never calls the loader", "[arc]") {
    cache_t<int> cache(0);
    int loads = 0;
    auto load = [&](int key) { ++loads; return key; };
    REQUIRE(cache.full());
    REQUIRE_FALSE(cache.lookup_update(1, load));
    REQUIRE_FALSE(cache.lookup_update(1, load));
    REQUIRE(loads == 0);
}

TEST_CASE("ARC: A full T1 discards its oldest page", "[arc]") {
    cache_t<int> cache{2};
    REQUIRE_FALSE(cache.lookup_update(1, load_page));
    REQUIRE_FALSE(cache.lookup_update(2, load_page));
    REQUIRE(cache.full());
    REQUIRE_FALSE(cache.lookup_update(3, load_page));
    REQUIRE_FALSE(cache.lookup_update(1, load_page));
    REQUIRE(cache.full_l1());
}

TEST_CASE("ARC: Hits in T1 and T2 do not call the loader", "[arc]") {
    cache_t<int> cache(2);
    int loads = 0;
    auto load = [&](int key) { ++loads; return key + 100; };
    REQUIRE_FALSE(cache.full());
    REQUIRE_FALSE(cache.lookup_update(5, load));
    REQUIRE(cache.lookup_update(5, load)); // T1 -> T2.
    REQUIRE(cache.lookup_update(5, load)); // Refresh MRU in T2.
    REQUIRE(loads == 1);
    REQUIRE_FALSE(cache.full());
    REQUIRE_FALSE(cache.lookup_update(7, load));
    REQUIRE(cache.full());
    REQUIRE(loads == 2);
}

TEST_CASE("ARC: One slot replaces pages from T1", "[arc]") {
    cache_t<int> cache(1);
    REQUIRE_FALSE(cache.lookup_update(1, load_page));
    REQUIRE(cache.full());
    REQUIRE_FALSE(cache.lookup_update(2, load_page));
    REQUIRE(cache.full());
    REQUIRE_FALSE(cache.lookup_update(1, load_page));
    REQUIRE(cache.lookup_update(1, load_page));
    REQUIRE(cache.lookup_update(1, load_page));
}

TEST_CASE("ARC: A T1 hit protects the page during a scan", "[arc]") {
    cache_t<int> cache(2);
    REQUIRE_FALSE(cache.lookup_update(1, load_page));
    REQUIRE(cache.lookup_update(1, load_page));
    // New pages cycle through T1; page 1 stays in T2.
    for (int key = 2; key <= 20; ++key) {
        REQUIRE_FALSE(cache.lookup_update(key, load_page));
        REQUIRE(cache.full());
    }
    REQUIRE(cache.lookup_update(1, load_page));
    REQUIRE(cache.lookup_update(20, load_page));
}

TEST_CASE("ARC: A T2 hit changes the next eviction victim", "[arc]") {
    cache_t<int> cache(2);
    REQUIRE_FALSE(cache.lookup_update(1, load_page));
    REQUIRE(cache.lookup_update(1, load_page));
    REQUIRE_FALSE(cache.lookup_update(2, load_page));
    REQUIRE(cache.lookup_update(2, load_page));
    REQUIRE(cache.lookup_update(1, load_page)); // T2: [1, 2].
    REQUIRE_FALSE(cache.lookup_update(3, load_page)); // Evict 2.
    REQUIRE(cache.lookup_update(1, load_page));
    REQUIRE_FALSE(cache.lookup_update(2, load_page));
}

TEST_CASE("ARC: A B1 return reloads the page and favors T1", "[arc]") {
    cache_t<int> cache(2);
    int loads = 0;
    auto load = [&](int key) { ++loads; return key + 100; };
    REQUIRE_FALSE(cache.lookup_update(1, load));
    REQUIRE(cache.lookup_update(1, load));
    REQUIRE_FALSE(cache.lookup_update(2, load));
    REQUIRE_FALSE(cache.lookup_update(3, load)); // T1: [3], T2: [1], B1: [2].
    REQUIRE(loads == 3);
    REQUIRE_FALSE(cache.lookup_update(2, load)); // p grows to 1; evict 1, keep 3.
    REQUIRE(loads == 4);
    REQUIRE(cache.lookup_update(2, load));
    REQUIRE(cache.lookup_update(3, load));
    REQUIRE(loads == 4);
    REQUIRE(cache.full());
}

TEST_CASE("ARC: A B2 return reloads the page and favors T2", "[arc]") {
    cache_t<int> cache(2);
    int loads = 0;
    auto load = [&](int key) { ++loads; return key; };
    REQUIRE_FALSE(cache.lookup_update(1, load));
    REQUIRE(cache.lookup_update(1, load));
    REQUIRE_FALSE(cache.lookup_update(2, load));
    REQUIRE_FALSE(cache.lookup_update(3, load));
    REQUIRE_FALSE(cache.lookup_update(2, load)); // p=1, T1: [3], T2: [2], B2: [1].
    REQUIRE_FALSE(cache.lookup_update(1, load)); // p=0; evict 3, keep 2.
    REQUIRE(loads == 5);
    REQUIRE(cache.lookup_update(1, load));
    REQUIRE(cache.lookup_update(2, load));
    REQUIRE(loads == 5);
    REQUIRE(cache.full());
}

TEST_CASE("ARC: A B2 return works when T1 is empty", "[arc]") {
    cache_t<int> cache(1);
    REQUIRE_FALSE(cache.lookup_update(1, load_page));
    REQUIRE(cache.lookup_update(1, load_page));
    REQUIRE_FALSE(cache.lookup_update(2, load_page)); // B2: [1], T1: [2].
    REQUIRE(cache.lookup_update(2, load_page)); // T1 empty, T2: [2], p=0.
    REQUIRE_FALSE(cache.lookup_update(1, load_page)); // Must evict from T2.
    REQUIRE(cache.lookup_update(1, load_page));
    REQUIRE(cache.full());
}

TEST_CASE("ARC: Full L1 forgets the oldest B1 key", "[arc]") {
    cache_t<int> cache(2);
    REQUIRE_FALSE(cache.lookup_update(1, load_page));
    REQUIRE(cache.lookup_update(1, load_page));
    REQUIRE_FALSE(cache.lookup_update(2, load_page));
    REQUIRE_FALSE(cache.lookup_update(3, load_page)); // B1: [2].
    REQUIRE(cache.full_l1());
    REQUIRE_FALSE(cache.lookup_update(4, load_page)); // Forget 2; B1: [3].
    REQUIRE_FALSE(cache.lookup_update(2, load_page)); // New key, not a B1 hit.
    REQUIRE(cache.lookup_update(1, load_page)); // p stayed 0; page 1 survived.
    REQUIRE(cache.lookup_update(2, load_page));
}

TEST_CASE("ARC: Full history forgets the oldest B2 key", "[arc]") {
    cache_t<int> cache(2);
    REQUIRE_FALSE(cache.lookup_update(1, load_page));
    REQUIRE(cache.lookup_update(1, load_page));
    REQUIRE_FALSE(cache.lookup_update(2, load_page));
    REQUIRE(cache.lookup_update(2, load_page));
    REQUIRE_FALSE(cache.lookup_update(3, load_page));
    REQUIRE(cache.lookup_update(3, load_page));
    REQUIRE_FALSE(cache.lookup_update(4, load_page));
    REQUIRE(cache.lookup_update(4, load_page)); // T2: [4, 3], B2: [2, 1].
    REQUIRE_FALSE(cache.lookup_update(5, load_page)); // Forget 1, evict 3.
    REQUIRE_FALSE(cache.lookup_update(1, load_page)); // New key -> T1.
    REQUIRE(cache.full_l1()); // T1: [1], B1: [5].
    REQUIRE(cache.lookup_update(4, load_page));
    REQUIRE(cache.lookup_update(1, load_page));
}

TEST_CASE("ARC: String keys are independent of integer values", "[arc]") {
    cache_t<int, std::string> cache(2);
    int loads = 0;
    auto load = [&](const std::string& key) { ++loads; return 100; };
    REQUIRE_FALSE(cache.lookup_update("one", load));
    REQUIRE(cache.lookup_update("one", load));
    REQUIRE_FALSE(cache.lookup_update("two", load));
    REQUIRE_FALSE(cache.lookup_update("three", load));
    REQUIRE_FALSE(cache.lookup_update("two", load)); // B1 return.
    REQUIRE_FALSE(cache.lookup_update("one", load)); // B2 return.
    REQUIRE(cache.lookup_update("one", load));
    REQUIRE(cache.lookup_update("two", load));
    REQUIRE(loads == 5);
}
