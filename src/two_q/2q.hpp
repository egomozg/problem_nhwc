#pragma once

#include <algorithm>
#include <cstddef>
#include <list>
#include <stdexcept>
#include <unordered_map>
#include <utility>

template <typename T, typename KeyT = int> struct cache_t {
private:
  std::size_t sz_;
  std::size_t Kin_;
  std::size_t Kout_;
  using Entry = std::pair<KeyT, T>;
  std::list<Entry> am_;
  std::list<Entry> a1_in_;
  std::list<KeyT> a1_out_;
  using ListIt = typename std::list<Entry>::iterator;
  std::unordered_map<KeyT, ListIt> am_hash_;
  std::unordered_map<KeyT, ListIt> a1_in_hash_;
  using GhostIt = typename std::list<KeyT>::iterator;
  std::unordered_map<KeyT, GhostIt> a1_out_hash_;

  // i think we should test this method exclusively
  // TODO: how to test private method...
  void reclaim() {
    if (!full())
      return;
    if (!a1_in_.empty() &&
        (a1_in_.size() > Kin_ || am_.empty())) {
      const auto& key = a1_in_.back().first;
      if (Kout_ != 0) {
        a1_out_.push_front(key);
        a1_out_hash_.emplace(key, a1_out_.begin());
        if (a1_out_.size() > Kout_) {
          a1_out_hash_.erase(a1_out_.back());
          a1_out_.pop_back();
        }
      }
      a1_in_hash_.erase(key);
      a1_in_.pop_back();
    } else {
      am_hash_.erase(am_.back().first);
      am_.pop_back();
    }
  }

public:
  cache_t(std::size_t sz)
      : sz_(sz), 
      Kin_(sz == 0 ? 0 : std::max<std::size_t>(1, sz / 4)),
      Kout_(sz == 0 ? 0 : std::max<std::size_t>(1, sz / 2)) {}

  bool full() const { return am_.size() + a1_in_.size() >= sz_; }

  template <typename F> bool lookup_update(KeyT key, F slow_get_page) {
    if (sz_ == 0)
      return false;

    auto hit = am_hash_.find(key);
    if (hit != am_hash_.end()) {
      am_.splice(am_.begin(), am_, hit->second);
      return true;
    }

    if (a1_in_hash_.find(key) != a1_in_hash_.end())
      return true;

    T page = slow_get_page(key);
    auto ghost = a1_out_hash_.find(key);
    if (ghost != a1_out_hash_.end()) {
      a1_out_.erase(ghost->second);
      a1_out_hash_.erase(ghost);
      reclaim();
      am_.emplace_front(key, page);
      am_hash_.emplace(key, am_.begin());
      return false;
    }

	reclaim();
	a1_in_.emplace_front(key, page);
	a1_in_hash_.emplace(key, a1_in_.begin());
    return false;
  }
};
