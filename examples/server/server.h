#pragma once

#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#if defined(_WIN32)
#    define NOMINMAX
#    include "WinSock2.h"
#    include <WS2tcpip.h>
#elif defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
#    include <arpa/inet.h>
#    include <netdb.h>
#    include <sys/socket.h>
#    include <unistd.h>

#    define SOCKET int
#    define INVALID_SOCKET -1
#    define SOCKET_ERROR -1
#endif

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
        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }

  private:
    std::queue<T> queue_;
    std::mutex    mutex_;
};

std::vector<std::thread> init_server();
void                     deinit_server(std::vector<std::thread>& threads);