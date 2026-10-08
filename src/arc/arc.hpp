#pragma once
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <list>
#include <unordered_map>
#include <utility>

template <typename T, typename KeyT = int>
class cache_t {
	std::size_t sz_;
	std::size_t p_ = 0;

	using Entry = std::pair<KeyT, T>;
	using ListIt = typename std::list<Entry>::iterator;
	using GhostIt = typename std::list<KeyT>::iterator;

	std::list<Entry> t1_;
	std::list<Entry> t2_;
	std::list<KeyT> b1_;
	std::list<KeyT> b2_;

	std::unordered_map<KeyT, ListIt> t1_hash_;
	std::unordered_map<KeyT, ListIt> t2_hash_;
	std::unordered_map<KeyT, GhostIt> b1_hash_;
	std::unordered_map<KeyT, GhostIt> b2_hash_;

	void replace(KeyT key) {
		bool in_b2 = b2_hash_.find(key) != b2_hash_.end();
		if (!t1_.empty() && (t1_.size() > p_ || (in_b2 && t1_.size() == p_))) {
			auto lru_t1_key = t1_.back().first;
			t1_hash_.erase(lru_t1_key);
			t1_.pop_back();
			b1_.emplace_front(lru_t1_key);
			b1_hash_.emplace(lru_t1_key, b1_.begin());
		} else {
			auto lru_t2_key = t2_.back().first;
			t2_hash_.erase(lru_t2_key);
			t2_.pop_back();
			b2_.emplace_front(lru_t2_key);
			b2_hash_.emplace(lru_t2_key, b2_.begin());
		}
	}

public:
	bool full_l1() const { return t1_.size() + b1_.size() == sz_; }
	cache_t(std::size_t cache_size) : sz_(cache_size) {};

	bool full() const { return t1_.size() + t2_.size() == sz_; }

	template <typename F>
	bool lookup_update(KeyT key, F slow_get_page) {
		if (sz_ == 0)
			return false;
		auto hit = t1_hash_.find(key);
		if (hit != t1_hash_.end()) {
			auto eltit = hit->second;
			t1_hash_.erase(hit);
			t2_.splice(t2_.begin(), t1_, eltit);
			t2_hash_.emplace(key, eltit);
			return true;
		}

		hit = t2_hash_.find(key);
		if (hit != t2_hash_.end()) {
			auto eltit = hit->second;
			t2_.splice(t2_.begin(), t2_, eltit);
			return true;
		}

		T page = slow_get_page(key);
		auto ghost = b1_hash_.find(key);
		if (ghost != b1_hash_.end()) {
			auto delta = std::max<std::size_t>(1, b2_.size() / b1_.size());
			p_ = p_ + std::min(delta, sz_ - p_);
			replace(key);
			b1_.erase(ghost->second);
			b1_hash_.erase(ghost);
			t2_.emplace_front(key, page);
			t2_hash_.emplace(key, t2_.begin());
			return false;
		}
		ghost = b2_hash_.find(key);
		if (ghost != b2_hash_.end()) {
			auto delta = std::max<std::size_t>(1, b1_.size() / b2_.size());
			p_ = p_ - std::min(delta, p_);
			replace(key);
			b2_.erase(ghost->second);
			b2_hash_.erase(ghost);
			t2_.emplace_front(key, page);
			t2_hash_.emplace(key, t2_.begin());
			return false;
		}

		if (full_l1()) {
			if (t1_.size() < sz_) {
				b1_hash_.erase(b1_.back());
				b1_.pop_back();
				replace(key);
			} else {
				t1_hash_.erase(t1_.back().first);
				t1_.pop_back();
			}
		} else {
			std::size_t l1_l2_size =
					t1_.size() + t2_.size() + b1_.size() + b2_.size();
			if (l1_l2_size >= sz_) {
				if (l1_l2_size == sz_ * 2) {
					b2_hash_.erase(b2_.back());
					b2_.pop_back();
				}
				replace(key);
			}
		}
		t1_.emplace_front(key, page);
		t1_hash_.emplace(key, t1_.begin());
		return false;
	}
};
