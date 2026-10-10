#pragma once

#include <cstddef>
#include <list>
#include <unordered_map>
#include <utility>

template <typename T, typename KeyT = int>
class cache_t {
	std::size_t sz_;
	using Entry = std::pair<KeyT, T>;
	std::list<Entry> cache_;

	using ListIt = typename std::list<Entry>::iterator;
	std::unordered_map<KeyT, ListIt> hash_;

public:
	cache_t(std::size_t sz) : sz_(sz) {};

	bool full() const { return cache_.size() >= sz_; }

	template <typename F>
	bool lookup_update(KeyT key, F slow_get_page) {
		if (sz_ == 0) {
			return false;
		}
		auto hit = hash_.find(key);
		if (hit != hash_.end()) {
			auto eltit = hit->second;
			cache_.splice(cache_.begin(), cache_, eltit);
			return true;
		}

		T page = slow_get_page(key);
		if (full()) {
			hash_.erase(cache_.back().first);
			cache_.pop_back();
		}

		cache_.emplace_front(key, page);
		hash_.emplace(key, cache_.begin());
		return false;
	}
};
