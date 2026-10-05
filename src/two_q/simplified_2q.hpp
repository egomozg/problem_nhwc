#pragma once

#include <cstddef>
#include <list>
#include <unordered_map>
#include <utility>

template <typename T, typename KeyT = int> class cache_t {
  std::size_t sz_;
  std::size_t a1_threshold_;
  using Entry = std::pair<KeyT, T>;
  std::list<Entry> am_;
  std::list<Entry> a1_;

  using ListIt = typename std::list<Entry>::iterator;
  std::unordered_map<KeyT, ListIt> am_hash_;
  std::unordered_map<KeyT, ListIt> a1_hash_;

public:
  cache_t(const std::size_t sz)
      : sz_(sz), a1_threshold_(sz == 0 ? 0 : (sz / 4 == 0 ? 1 : sz / 4)) {};

  bool full() const { return am_.size() + a1_.size() >= sz_; }

  template <typename F> bool lookup_update(KeyT key, F slow_get_page) {
    if (sz_ == 0)
      return false;

    auto hit = am_hash_.find(key);
    if (hit != am_hash_.end()) {
      auto eltit = hit->second;
      am_.splice(am_.begin(), am_, eltit);
      return true;
    }

    auto a1_hit = a1_hash_.find(key);
    if (a1_hit != a1_hash_.end()) {
      auto eltit = a1_hit->second;
      am_.splice(am_.begin(), a1_, eltit);
      a1_hash_.erase(a1_hit);
      am_hash_.emplace(key, eltit);
      return true;
    }

    T page = slow_get_page(key);
    if (full()) {
      if (!a1_.empty() && (a1_.size() > a1_threshold_ || am_.empty())) {
        a1_hash_.erase(a1_.back().first);
        a1_.pop_back();
      } else {
        am_hash_.erase(am_.back().first);
        am_.pop_back();
      }
    }

    a1_.emplace_front(key, page);
    a1_hash_.emplace(key, a1_.begin());
    return false;
  }
};
