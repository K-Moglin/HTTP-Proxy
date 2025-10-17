\
#pragma once
#include <string>
#include "thread_pool.h"
#include "lru_cache.h"
#include "platform_sockets.h"

class ProxyServer {
public:
  ProxyServer(const std::string& addr, const std::string& port, size_t workers, size_t cache_bytes);
  void run();
private:
  std::string addr_, port_;
  ThreadPool pool_;
  LruCache cache_;
};
