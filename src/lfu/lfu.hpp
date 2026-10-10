#pragma once

#include <cstddef>
#include <iterator>
#include <list>
#include <unordered_map>
#include <utility>

template <typename T, typename KeyT = int>
class cache_t {
	std::size_t sz_;

	using Entry = std::pair<KeyT, T>;
	struct FreqBucket {
		std::size_t freq;
		std::list<Entry> entries;
	};
	std::list<FreqBucket> freq_;

	using FreqIt = typename std::list<FreqBucket>::iterator;
	using ListIt = typename std::list<Entry>::iterator;
	struct Position {
		FreqIt bucket;
		ListIt entry;
	};

	std::unordered_map<KeyT, Position> hash_;

	void promote(Position &pos) {
		auto bucket_it = pos.bucket;
		auto bucket_next = std::next(bucket_it);
		const auto new_freq = bucket_it->freq + 1;

		if (bucket_next == freq_.end() || bucket_next->freq != new_freq) {
			bucket_next = freq_.insert(bucket_next, FreqBucket{new_freq, {}});
		}
		bucket_next->entries.splice(bucket_next->entries.begin(),
																bucket_it->entries, pos.entry);
		pos.bucket = bucket_next;

		if (bucket_it->entries.empty()) {
			freq_.erase(bucket_it);
		}
	}

	void evict() {
		auto bucket = freq_.begin();
		hash_.erase(bucket->entries.back().first);
		bucket->entries.pop_back();
		if (bucket->entries.empty()) {
			freq_.erase(bucket);
		}
	}

	void insert_new(KeyT key, T page) {
		if (freq_.empty() || freq_.front().freq != 1) {
			freq_.push_front(FreqBucket{1, {}});
		}

		auto bucket = freq_.begin();
		bucket->entries.emplace_front(key, page);
		hash_.emplace(key, Position{bucket, bucket->entries.begin()});
	}

public:
	cache_t(std::size_t sz) : sz_(sz) {};
	bool full() const { return hash_.size() >= sz_; }

	template <typename F>
	bool lookup_update(KeyT key, F slow_get_page) {
		if (sz_ == 0) {
			return false;
		}

		auto hit = hash_.find(key);
		if (hit != hash_.end()) {
			promote(hit->second);
			return true;
		}

		T page = slow_get_page(key);

		if (full()) {
			evict();
		}

		insert_new(key, page);
		return false;
	}
};
