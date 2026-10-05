#include "lfu.hpp"

#include <catch2/catch_test_macros.hpp>
#include <map>

namespace {
int load_page(int key) { return key; }
}

TEST_CASE("LFU: Zero capacity never calls the loader", "[lfu]") {
    cache_t<int> cache(0);
    int loads = 0;
    auto load = [&](int key) { ++loads; return key; };
    REQUIRE(cache.full());
    REQUIRE_FALSE(cache.lookup_update(1, load));
    REQUIRE_FALSE(cache.lookup_update(1, load));
    REQUIRE(loads == 0);
}

TEST_CASE("LFU: One slot supports promotion and replacement", "[lfu]") {
    cache_t<int> cache(1);
    int loads = 0;
    auto load = [&](int key) { ++loads; return key; };
    REQUIRE_FALSE(cache.full());
    REQUIRE_FALSE(cache.lookup_update(1, load));
    REQUIRE(cache.full());
    for (int i = 0; i < 10; ++i) {
        REQUIRE(cache.lookup_update(1, load));
    }
    REQUIRE(loads == 1);
    REQUIRE_FALSE(cache.lookup_update(2, load));
    REQUIRE(cache.lookup_update(2, load));
    REQUIRE_FALSE(cache.lookup_update(1, load));
    REQUIRE(loads == 3);
}

TEST_CASE("LFU: Frequency wins over recency", "[lfu]") {
    cache_t<int> cache(2);
    REQUIRE_FALSE(cache.lookup_update(1, load_page));
    REQUIRE(cache.lookup_update(1, load_page));
    REQUIRE(cache.lookup_update(1, load_page));
    REQUIRE_FALSE(cache.lookup_update(2, load_page));
    REQUIRE_FALSE(cache.lookup_update(3, load_page));
    REQUIRE(cache.lookup_update(1, load_page));
    REQUIRE(cache.lookup_update(3, load_page));
    REQUIRE_FALSE(cache.lookup_update(2, load_page));
}

TEST_CASE("LFU: Equal frequencies evict the least recently used page", "[lfu]") {
    cache_t<int> cache(2);
    REQUIRE_FALSE(cache.lookup_update(1, load_page));
    REQUIRE_FALSE(cache.lookup_update(2, load_page));
    REQUIRE(cache.lookup_update(2, load_page));
    REQUIRE(cache.lookup_update(1, load_page));
    REQUIRE_FALSE(cache.lookup_update(3, load_page));
    REQUIRE(cache.lookup_update(1, load_page));
    REQUIRE_FALSE(cache.lookup_update(2, load_page));
}
