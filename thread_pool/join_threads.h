#pragma once

#include <vector>
#include <thread>

class join_threads {
    std::vector<std::thread> &threads_;

  public:
    explicit join_threads(std::vector<std::thread> &threads) : threads_(threads) {}

    ~join_threads() {
        for (int i = 0; i < (int) threads_.size(); ++i) {
            if (threads_[i].joinable()) {
                threads_[i].join();
            }
        }
    }
};
