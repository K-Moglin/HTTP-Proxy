\
#pragma once
#include <unordered_map>
#include <list>
#include <string>
#include <chrono>
#include <shared_mutex>

struct CacheEntry {
  std::string key;
  std::string value;
  std::string etag;
  std::string last_modified;
  std::chrono::steady_clock::time_point expiry{};
  size_t bytes = 0;
};

class LruCache {
public:
  explicit LruCache(size_t capacity_bytes) : cap_(capacity_bytes), size_(0) {}

  bool get(const std::string &key, CacheEntry &out){
    std::shared_lock lk(mu_);
    auto it = map_.find(key);
    if (it == map_.end()) return false;
    lru_.splice(lru_.begin(), lru_, it->second);
    out = *(it->second);
    return true;
  }
  
  void put(CacheEntry entry){
    std::unique_lock lk(mu_);
    auto it = map_.find(entry.key);
    if (it != map_.end()){
      size_ -= it->second->bytes;
      lru_.erase(it->second);
      map_.erase(it);
    }
    lru_.push_front(std::move(entry));
    map_[lru_.front().key] = lru_.begin();
    size_ += lru_.front().bytes;
    while (size_ > cap_ && !lru_.empty()){
      size_ -= lru_.back().bytes;
      map_.erase(lru_.back().key);
      lru_.pop_back();
    }
  }

private:
  size_t cap_;
  size_t size_;
  std::list<CacheEntry> lru_;
  std::unordered_map<std::string, std::list<CacheEntry>::iterator> map_;
  mutable std::shared_mutex mu_;
};
