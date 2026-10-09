#include "lirs.hpp"

#include <catch2/catch_test_macros.hpp>
#include <string>

namespace {

int loads = 0;

int load_page(int key) {
	++loads;
	return key;
}

int load_string_page(const std::string &key) {
	return static_cast<int>(key.size());
}

int pages_alive = 0;

struct Page {
	int value;

	explicit Page(int key) : value(key) { ++pages_alive; }
	Page(const Page &other) : value(other.value) { ++pages_alive; }
	~Page() { --pages_alive; }
};

Page load_tracked_page(int key) { return Page(key); }

} // namespace

TEST_CASE("LIRS: Zero capacity never calls the loader", "[lirs]") {
	cache_t<int> cache(0);
	loads = 0;
	REQUIRE(cache.full());
	REQUIRE_FALSE(cache.lookup_update(1, load_page));
	REQUIRE_FALSE(cache.lookup_update(1, load_page));
	REQUIRE(loads == 0);
}

TEST_CASE("LIRS: Warm-up fills the cache and hits do not reload", "[lirs]") {
	cache_t<int> cache(3);
	loads = 0;
	REQUIRE_FALSE(cache.full());
	REQUIRE_FALSE(cache.lookup_update(1, load_page));
	REQUIRE(cache.lookup_update(1, load_page));
	REQUIRE_FALSE(cache.lookup_update(2, load_page));
	REQUIRE_FALSE(cache.full());
	REQUIRE_FALSE(cache.lookup_update(3, load_page));
	REQUIRE(cache.full());
	REQUIRE(cache.lookup_update(3, load_page));
	REQUIRE(loads == 3);
}

TEST_CASE("LIRS: One slot replaces pages without a LIR part", "[lirs]") {
	cache_t<int> cache(1);
	loads = 0;
	REQUIRE_FALSE(cache.lookup_update(1, load_page));
	REQUIRE(cache.full());
	REQUIRE(cache.lookup_update(1, load_page));
	REQUIRE_FALSE(cache.lookup_update(2, load_page));
	REQUIRE(cache.lookup_update(2, load_page));
	REQUIRE_FALSE(cache.lookup_update(1, load_page));
	REQUIRE(loads == 3);
}

TEST_CASE("LIRS: A scan preserves LIR pages", "[lirs]") {
	cache_t<int> cache(3);
	REQUIRE_FALSE(cache.lookup_update(1, load_page));
	REQUIRE_FALSE(cache.lookup_update(2, load_page));
	for (int key = 3; key <= 100; ++key) {
		REQUIRE_FALSE(cache.lookup_update(key, load_page));
		REQUIRE(cache.full());
	}
	REQUIRE(cache.lookup_update(1, load_page));
	REQUIRE(cache.lookup_update(2, load_page));
	REQUIRE(cache.lookup_update(100, load_page));
}

TEST_CASE("LIRS: A resident HIR in S is promoted", "[lirs]") {
	cache_t<int> cache(3);
	for (int key = 1; key <= 3; ++key)
		REQUIRE_FALSE(cache.lookup_update(key, load_page));
	REQUIRE(cache.lookup_update(3, load_page));				// Promote 3, demote 1.
	REQUIRE_FALSE(cache.lookup_update(4, load_page)); // Evict 1.
	REQUIRE(cache.lookup_update(2, load_page));
	REQUIRE(cache.lookup_update(3, load_page));
	REQUIRE_FALSE(cache.lookup_update(1, load_page));
}

TEST_CASE("LIRS: Returning history is a miss but promotes the page", "[lirs]") {
	cache_t<int> cache(3);
	loads = 0;
	for (int key = 1; key <= 4; ++key)
		REQUIRE_FALSE(cache.lookup_update(key, load_page));
	REQUIRE_FALSE(cache.lookup_update(3, load_page)); // Ghost 3 replaces 4.
	REQUIRE(loads == 5);
	REQUIRE_FALSE(cache.lookup_update(5, load_page)); // Evict demoted 1.
	REQUIRE(cache.lookup_update(2, load_page));
	REQUIRE(cache.lookup_update(3, load_page));
	REQUIRE(loads == 6);
	REQUIRE_FALSE(cache.lookup_update(1, load_page));
}

TEST_CASE("LIRS: Pruning S keeps resident HIR data", "[lirs]") {
	cache_t<int> cache(3);
	for (int key = 1; key <= 3; ++key)
		REQUIRE_FALSE(cache.lookup_update(key, load_page));
	REQUIRE(cache.lookup_update(1, load_page));
	REQUIRE(cache.lookup_update(2, load_page)); // Remove 3 from S, keep in Q.
	REQUIRE(cache.lookup_update(3, load_page)); // Back in S, still HIR.
	REQUIRE_FALSE(cache.lookup_update(4, load_page)); // Evict 3.
	REQUIRE(cache.lookup_update(1, load_page));
	REQUIRE(cache.lookup_update(2, load_page));
	REQUIRE_FALSE(cache.lookup_update(3, load_page));
}

TEST_CASE("LIRS: Pruned history does not cause promotion", "[lirs]") {
	cache_t<int> cache(3);
	for (int key = 1; key <= 4; ++key)
		REQUIRE_FALSE(cache.lookup_update(key, load_page));
	REQUIRE(cache.lookup_update(1, load_page));
	REQUIRE(cache.lookup_update(2, load_page));				// Forget ghost 3.
	REQUIRE_FALSE(cache.lookup_update(3, load_page)); // New HIR, not LIR.
	REQUIRE_FALSE(cache.lookup_update(5, load_page)); // Evict 3.
	REQUIRE(cache.lookup_update(1, load_page));
	REQUIRE(cache.lookup_update(2, load_page));
}

TEST_CASE("LIRS: A demoted page needs two accesses to become LIR again",
					"[lirs]") {
	cache_t<int> cache(3);
	for (int key = 1; key <= 3; ++key)
		REQUIRE_FALSE(cache.lookup_update(key, load_page));
	REQUIRE(cache.lookup_update(3, load_page));				// Demote 1, remove from S.
	REQUIRE(cache.lookup_update(1, load_page));				// Add 1 to S, still HIR.
	REQUIRE(cache.lookup_update(1, load_page));				// Promote 1, demote 2.
	REQUIRE_FALSE(cache.lookup_update(4, load_page)); // Evict 2.
	REQUIRE(cache.lookup_update(1, load_page));
	REQUIRE(cache.lookup_update(3, load_page));
	REQUIRE_FALSE(cache.lookup_update(2, load_page));
}

TEST_CASE("LIRS: HIR hits outside S refresh Q eviction order", "[lirs]") {
	cache_t<int> cache(200); // 198 LIR slots and 2 HIR slots.
	for (int key = 1; key <= 200; ++key)
		REQUIRE_FALSE(cache.lookup_update(key, load_page));
	for (int key = 1; key <= 198; ++key)
		REQUIRE(cache.lookup_update(key, load_page));			// Prune 199 and 200.
	REQUIRE(cache.lookup_update(199, load_page));				// Q: [199, 200].
	REQUIRE_FALSE(cache.lookup_update(201, load_page)); // Evict 200.
	REQUIRE(cache.lookup_update(199, load_page));
	REQUIRE_FALSE(cache.lookup_update(200, load_page));
}

TEST_CASE("LIRS: Demotion adds a page to the recent end of Q", "[lirs]") {
	cache_t<int> cache(200);
	for (int key = 1; key <= 200; ++key)
		REQUIRE_FALSE(cache.lookup_update(key, load_page));
	REQUIRE(cache.lookup_update(199, load_page));				// Q: [1, 200].
	REQUIRE_FALSE(cache.lookup_update(201, load_page)); // Evict 200, not 1.
	REQUIRE(cache.lookup_update(1, load_page));
	REQUIRE_FALSE(cache.lookup_update(200, load_page));
}

TEST_CASE("LIRS: String keys are independent of integer values", "[lirs]") {
	cache_t<int, std::string> cache(2);
	REQUIRE_FALSE(cache.lookup_update("one", load_string_page));
	REQUIRE_FALSE(cache.lookup_update("two", load_string_page));
	REQUIRE_FALSE(cache.lookup_update("three", load_string_page));
	REQUIRE_FALSE(cache.lookup_update("two", load_string_page));
	REQUIRE(cache.lookup_update("two", load_string_page));
	REQUIRE(
			cache.lookup_update("one", load_string_page)); // Demoted, still resident.
	REQUIRE_FALSE(cache.lookup_update("four", load_string_page)); // Evict one.
	REQUIRE_FALSE(cache.lookup_update("one", load_string_page));
}

TEST_CASE("LIRS: History retains no pages and data is not duplicated",
					"[lirs]") {
	REQUIRE(pages_alive == 0);
	{
		cache_t<Page> cache(3);
		for (int key = 1; key <= 100; ++key) {
			REQUIRE_FALSE(cache.lookup_update(key, load_tracked_page));
			REQUIRE(pages_alive == (key < 3 ? key : 3));
		}
		REQUIRE_FALSE(cache.lookup_update(50, load_tracked_page));
		REQUIRE(cache.lookup_update(50, load_tracked_page));
		REQUIRE(pages_alive == 3);
	}
	REQUIRE(pages_alive == 0);
}
