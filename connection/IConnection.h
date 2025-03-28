#pragma once

#include <memory>

enum connection_sender_status { OK = 0, DISPLACEMENT_IN_QUEUE = 1, NO_RECEIVERS = 2, ERROR = -1 };

template <typename T> class IConnectionReceiver {

  public:
    virtual ~IConnectionReceiver() = default;

    virtual std::shared_ptr<T> receive() = 0;

    virtual std::shared_ptr<T> waitAndReceive() = 0;

    virtual void close() = 0;

    virtual std::shared_ptr<IConnectionReceiver<T>> copy() = 0;

    virtual int getCapacity() = 0;
};

template <typename T> class IConnectionSender {

  public:
    virtual ~IConnectionSender() = default;

    virtual int send(const std::shared_ptr<T> &val) = 0;

    virtual int send(const T &val) = 0;

    virtual std::shared_ptr<IConnectionReceiver<T>> getReceiver() = 0;

    virtual void close() = 0;

    virtual std::shared_ptr<IConnectionSender<T>> copy() = 0;

    virtual int getCapacity() = 0;
};

template <typename T> using rx_connection_ptr = std::shared_ptr<IConnectionReceiver<T>>;

template <typename T> using tx_connection_ptr = std::shared_ptr<IConnectionSender<T>>;
