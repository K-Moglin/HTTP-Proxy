\
#pragma once
#include <string>
#include "lru_cache.h"
#include "thread_pool.h"
#include "platform_sockets.h"

class Connection {
public:
  Connection(socket_t client_fd, LruCache& cache, ThreadPool& pool);
  void operator()();
private:
  socket_t cfd_;
  LruCache& cache_;
  ThreadPool& pool_;
  bool handle_one_request();
  bool handle_connect(const std::string& host, const std::string& port);
  bool handle_regular(const std::string& head);
};
