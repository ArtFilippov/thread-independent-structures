#pragma once

#include <mutex>
#include <condition_variable>
#include <memory>
#include <queue>

template <typename T> class threadsafe_queue {

    mutable std::mutex mut;
    std::queue<std::shared_ptr<T>> data;
    std::condition_variable cond;

  public:
    const threadsafe_queue &operator=(const threadsafe_queue &) = delete;

    /**
     * @brief
     * - Если очередь не пуста, перемещает (std::move) `front` элемент в аргумент `value`
     *
     * - Если очередь пуста, вызывающий поток блокируется, пока другой поток не поместит новый элемент
     *
     * - Время O(1)
     * @param value Ссылка на переменную, где окажется `front` элемент очереди
     */
    void wait_and_pop(T &value) {
        std::unique_lock<std::mutex> lk(mut);
        cond.wait(lk, [this] { return !data.empty(); });
        value = std::move(*data.front());
        data.pop();
    }

    /**
     * @brief
     * - Если очередь не пуста, извлекает `front` элемент из очереди
     *
     * - Если очередь пуста, вызывающий поток блокируется, пока другой поток не поместит новый элемент
     *
     * - Время O(1)
     * @return `std::shared_ptr`, ссылающийся на бывший `front` элемент очереди
     */
    std::shared_ptr<T> wait_and_pop() {
        std::unique_lock<std::mutex> lk(mut);
        cond.wait(lk, [this] { return !data.empty(); });
        std::shared_ptr<T> value = data.front();
        data.pop();
        return value;
    }

    /**
     * @brief
     * - Если очередь не пуста, перемещает (std::move) `front` элемент в аргумент `value`
     *
     * - Если очередь пуста, немедленно возвращает `false`
     *
     * - Время O(1)
     * @param value Ссылка на переменную, где окажется `front` элемент очереди
     * @return
     *  - `true`, если значение успешно передано в `value`
     *
     *  - `false`, если очередь пуста
     */
    bool try_pop(T &value) {
        std::lock_guard<std::mutex> lg(mut);
        if (data.empty()) {
            return false;
        }
        value = std::move(*data.front());
        data.pop();
        return true;
    }

    /**
     * @brief
     * - Если очередь не пуста, извлекает `front` элемент из очереди
     *
     * - Если очередь пуста, немедленно возвращает `std::shared_ptr<T>(nullptr)`
     *
     * - Время O(1)
     * @return
     *  - `std::shared_ptr`, ссылающийся на бывший `front` элемент очереди, если очередь не была пуста
     *
     *  - `std::shared_ptr<T>(nullptr)`, если очередь пуста
     */
    std::shared_ptr<T> try_pop() {
        std::lock_guard<std::mutex> lg(mut);
        if (data.empty()) {
            return {nullptr};
        }
        std::shared_ptr<T> value = data.front();
        data.pop();
        return value;
    }

    /**
     * @brief
     * - Оборачивает `value` в `std::shared_ptr` и добавляет его в очередь
     */
    void push(T value) {
        // нельзя принимать значение по ссылке, так как в этом случае оригинальный объект будет перемещён.
        // при этом, если необходимо переместить объект, вызываем push(std::move(value)) и вызывается конструктор
        // копирования перемещением.
        std::shared_ptr<T> new_value(std::make_shared<T>(std::move(value)));

        std::lock_guard<std::mutex> lg(mut);
        data.push(new_value);
        cond.notify_one();
    }

    /**
     * @brief
     * - Помещает в очередь уже обёрнутое в `std::shared_ptr` значение
     */
    void push(const std::shared_ptr<T> &value) {
        std::lock_guard<std::mutex> lg(mut);
        data.push(value);
        cond.notify_one();
    }

    /**
     * @return
     * - `true` пусто
     * - `false` не пусто
     */
    bool empty() const {
        std::lock_guard<std::mutex> lg(mut);
        return data.empty();
    }
};
