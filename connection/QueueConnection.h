#pragma once

#include "IConnection.h"
#include "../safe_queue/cyclic_queue.h"

template <typename T> class QueueConnectionSender : public IConnectionSender<T> {

    struct ConnectionBase {
        cyclic_queue<T> data;
        int capacity;

        std::atomic_int receiverCounter{0};
        std::atomic_int senderCounter{1};

        ConnectionBase(int qCapacity) : data(qCapacity), capacity(qCapacity) {}
    };

    std::shared_ptr<ConnectionBase> base;

    std::atomic_bool is_closed;

  public:
    class QueueConnectionReceiver : public IConnectionReceiver<T> {

        std::shared_ptr<ConnectionBase> base{nullptr};

        std::atomic_bool is_closed;

      public:
        QueueConnectionReceiver(std::shared_ptr<ConnectionBase> base) : base(base), is_closed(false) {
            if (base) {
                base->receiverCounter.fetch_add(1);
            }
        }

        QueueConnectionReceiver(QueueConnectionReceiver &other) : base(other.base), is_closed(false) {
            if (base) {
                base->receiverCounter.fetch_add(1);
            }
        }

        QueueConnectionReceiver(QueueConnectionReceiver &&other)
            : base(std::move(other.base)), is_closed(other.is_closed.load()) {}

        ~QueueConnectionReceiver() { close(); }

        std::shared_ptr<T> receive() override {
            if (!base) {
                return {nullptr};
            }

            auto val = base->data.try_pop();

            if (val) {
                return val;
            }

            if (base->senderCounter <= 0) {
                throw std::logic_error("the sender is closed, there will be no more data");
            }

            return {nullptr};
        }

        std::shared_ptr<T> waitAndReceive() override {
            std::shared_ptr<T> inst = base->data.wait_and_pop();
            if (inst) {
                return inst;
            } else {
                throw std::logic_error{"wait and receive disabled"};
            }
        }

        void close() override {
            bool current = false;

            if (is_closed.compare_exchange_strong(current, true)) {
                if (base) {
                    base->receiverCounter.fetch_sub(1);
                }
            }
        }

        std::shared_ptr<IConnectionReceiver<T>> copy() override {
            std::shared_ptr<IConnectionReceiver<T>> new_receiver = std::make_shared<QueueConnectionReceiver>(*this);

            return new_receiver;
        }

        int getCapacity() override {
            if (base) {
                return base->capacity;
            } else {
                return 0;
            }
        }
    };

    QueueConnectionSender(int queueCapacity) : base(new ConnectionBase(queueCapacity)) {}

    QueueConnectionSender(QueueConnectionSender &other) : base(other.base), is_closed(false) {
        if (base) {
            base->senderCounter.fetch_add(1);
        }
    }

    QueueConnectionSender(QueueConnectionSender &&other)
        : base(std::move(other.base)), is_closed(other.is_closed.load()) {}

    int send(const T &frame) override {
        if (!base) {
            return connection_sender_status::ERROR;
        }

        int res = (base->receiverCounter <= 0) ? connection_sender_status::NO_RECEIVERS : connection_sender_status::OK;

        if (base->data.push(frame) == queue_status::PUSH_WITH_DISPLACEMENT) {
            res |= connection_sender_status::DISPLACEMENT_IN_QUEUE;
        }

        return res;
    }

    int send(const std::shared_ptr<T> &val) {
        if (!base) {
            return connection_sender_status::ERROR;
        }

        int res = (base->receiverCounter <= 0) ? connection_sender_status::NO_RECEIVERS : connection_sender_status::OK;

        if (base->data.push(val) == queue_status::PUSH_WITH_DISPLACEMENT) {
            res |= connection_sender_status::DISPLACEMENT_IN_QUEUE;
        }

        return res;
    }

    std::shared_ptr<IConnectionReceiver<T>> getReceiver() override {
        return std::shared_ptr<QueueConnectionReceiver>(new QueueConnectionReceiver(base));
    }

    void close() override {
        bool current = false;

        if (is_closed.compare_exchange_strong(current, true)) {
            if (base) {
                base->senderCounter.fetch_sub(1);
            }
        }
    }

    int getCapacity() override {
        if (base) {
            return base->capacity;
        } else {
            return 0;
        }
    }

    std::shared_ptr<IConnectionSender<T>> copy() override {
        std::shared_ptr<IConnectionSender<T>> new_sender = std::make_shared<QueueConnectionSender<T>>(*this);

        return new_sender;
    }
};
