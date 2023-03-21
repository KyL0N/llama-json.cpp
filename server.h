#pragma once

#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

template <typename T> class ThreadSafeQueue {
  public:
    void push(const T& value)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(value);
    }

    bool tryPop(T& value)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        value = queue_.front();
        queue_.pop();
        return true;
    }

  private:
    std::queue<T> queue_;
    std::mutex    mutex_;
};

std::vector<std::thread> init_server();
void                     deinit_server(std::vector<std::thread>& threads);
