#pragma once

#include <mutex>
#include <condition_variable>
#include <memory>
#include <queue>

template <typename T> class threadsafe_queue {
    std::mutex mut;
    std::queue<std::shared_ptr<T>> data;
    std::condition_variable cond;

  public:
    void wait_and_pop(T &value) {
        std::unique_lock<std::mutex> lk(mut);
        cond.wait(lk, [this] { return !data.empty(); });
        value = std::move(*data.front());
        data.pop();
    }

    std::shared_ptr<T> wait_and_pop() {
        std::unique_lock<std::mutex> lk(mut);
        cond.wait(lk, [this] { return !data.empty(); });
        std::shared_ptr<T> value = data.front();
        data.pop();
        return value;
    }

    bool try_pop(T &value) {
        std::lock_guard<std::mutex> lg(mut);
        if (data.empty()) {
            return false;
        }
        value = std::move(*data.front());
        data.pop();
        return true;
    }

    std::shared_ptr<T> try_pop() {
        std::lock_guard<std::mutex> lg(mut);
        if (data.empty()) {
            return NULL;
        }
        std::shared_ptr<T> value = data.front();
        data.pop();
        return value;
    }

    void push(T value) {
        std::shared_ptr<T> new_value(std::make_shared<T>(std::move(value)));

        std::lock_guard<std::mutex> lg(mut);
        data.push(new_value);
        cond.notify_one();
    }

    void push(std::shared_ptr<T> value) {
        std::lock_guard<std::mutex> lg(mut);
        data.push(value);
        cond.notify_one();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lg(mut);
        return data.empty();
    }
};
