#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <list>
#include <unordered_map>

template <typename T, typename KeyT = int>
class cache_t {
	std::size_t sz_;
	std::size_t lir_capacity_;
	std::size_t lir_size_ = 0;
	enum class InterReference { LIR, HIR };

	using ListIt = typename std::list<KeyT>::iterator;
	struct Record {
		InterReference set = InterReference::HIR;
		bool in_s = false;
		bool in_q = false;
		ListIt s_pos;
		ListIt q_pos;
	};
	std::list<KeyT> s_;
	std::list<KeyT> q_;

	std::unordered_map<KeyT, Record> records_;
	std::unordered_map<KeyT, T> data_;

	void touch_s(KeyT key, Record &record) {
		if (record.in_s) {
			s_.splice(s_.begin(), s_, record.s_pos);
		} else {
			s_.push_front(key);
			record.s_pos = s_.begin();
			record.in_s = true;
		}
	}

	void touch_q(KeyT key, Record &record) {
		if (record.in_q) {
			q_.splice(q_.begin(), q_, record.q_pos);
		} else {
			q_.push_front(key);
			record.q_pos = q_.begin();
			record.in_q = true;
		}
		assert(data_.find(key) != data_.end());
	}

	void prune_s() {
		while (!s_.empty()) {
			auto record = records_.find(s_.back());
			if (record->second.set == InterReference::LIR)
				break;

			record->second.in_s = false;
			s_.pop_back();
			if (!record->second.in_q)
				records_.erase(record);
		}
	}

	void evict() {
		auto record = records_.find(q_.back());
		data_.erase(record->first);
		q_.pop_back();
		record->second.in_q = false;
		if (!record->second.in_s)
			records_.erase(record);
	}

	void promote(Record &record) {
		if (record.in_q) {
			q_.erase(record.q_pos);
			record.in_q = false;
		}
		record.set = InterReference::LIR;

		auto oldest = records_.find(s_.back());
		oldest->second.set = InterReference::HIR;
		touch_q(oldest->first, oldest->second);
		prune_s();
	}

public:
	cache_t(std::size_t capacity) : sz_(capacity), lir_capacity_(0) {
		if (sz_ != 0)
			lir_capacity_ = sz_ - std::max<std::size_t>(1, sz_ / 100);
	}

	bool full() const { return data_.size() >= sz_; }

	template <typename F>
	bool lookup_update(KeyT key, F slow_get_page) {
		if (sz_ == 0)
			return false;

		bool hit = data_.find(key) != data_.end();
		if (!hit) {
			T page = slow_get_page(key);
			if (full())
				evict();
			data_.emplace(key, page);
		}

		auto &record = records_[key];
		bool was_in_s = record.in_s;
		touch_s(key, record);

		if (record.set == InterReference::LIR) {
			prune_s();
		} else if (lir_size_ < lir_capacity_) {
			record.set = InterReference::LIR;
			++lir_size_;
		} else if (was_in_s && lir_capacity_ != 0) {
			promote(record);
		} else {
			touch_q(key, record);
			prune_s();
		}
		return hit;
	}
};
