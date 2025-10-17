\
#pragma once
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

class ThreadPool {
public:
  explicit ThreadPool(size_t n) : stop_(false) {
    for (size_t i = 0; i < n; ++i) {
      workers_.emplace_back([this]{ workerLoop(); });
    }
  }
  ~ThreadPool(){
    {
      std::lock_guard<std::mutex> lg(mu_);
      stop_ = true;
    }
    cv_.notify_all();
    for (auto &t : workers_) if (t.joinable()) t.join();
  }
  void enqueue(std::function<void()> fn){
    {
      std::lock_guard<std::mutex> lg(mu_);
      tasks_.push(std::move(fn));
    }
    cv_.notify_one();
  }
private:
  void workerLoop(){
    for(;;){
      std::function<void()> job;
      {
        std::unique_lock<std::mutex> lk(mu_);
        cv_.wait(lk, [&]{ return stop_ || !tasks_.empty(); });
        if (stop_ && tasks_.empty()) return;
        job = std::move(tasks_.front()); tasks_.pop();
      }
      try { job(); } catch (...) { }
    }
  }
  std::vector<std::thread> workers_;
  std::queue<std::function<void()>> tasks_;
  std::mutex mu_;
  std::condition_variable cv_;
  bool stop_;
};
