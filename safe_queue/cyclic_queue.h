#pragma once

#include "threadsafe_queue.h"

template <typename T> class cyclic_queue : public threadsafe_queue<T> {
    const int capacity;

  public:
    cyclic_queue(int capacity) : capacity(capacity) {}

    /**
     * @brief
     * - Оборачивает `value` в `std::shared_ptr` и добавляет его в очередь
     */
    int push(T value) override {
        // нельзя принимать значение по ссылке, так как в этом случае оригинальный объект будет перемещён.
        // при этом, если необходимо переместить объект, вызываем push(std::move(value)) и вызывается конструктор
        // копирования перемещением.
        std::shared_ptr<T> new_value(std::make_shared<T>(std::move(value)));

        return cyclic_queue::push(new_value);
    }

    /**
     * @brief
     * - Помещает в очередь уже обёрнутое в `std::shared_ptr` значение
     */
    int push(const std::shared_ptr<T> &value) override {
        std::lock_guard<std::mutex> lg{this->mut};

        auto res = queue_status::PUSH_OK;

        if (this->data.size() >= capacity) {
            this->data.pop();
            res = queue_status::PUSH_WITH_DISPLACEMENT;
        }

        this->data.push(value);
        this->cond.notify_one();

        return res;
    }
};
