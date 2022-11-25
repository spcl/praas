#ifndef PRAAS_PROCESS_CONTROLLER_HPP
#define PRAAS_PROCESS_CONTROLLER_HPP

#include <praas/process/controller/config.hpp>
#include <praas/process/controller/workers.hpp>
#include <praas/process/ipc/ipc.hpp>

#include <memory>

namespace praas::process {

  struct Controller {

    // State

    // Messages

    // Workers (IPC)

    // Func queue

    // Swapper object

    // TCP Poller object

    // lock-free queue from TCP server
    // lock-free queue to TCP server

    Controller(config::Controller cfg);

    ~Controller();

    void poll();

    void start();

    void shutdown();

  private:

    int _epoll_fd;

    Workers _workers;

    // Queue storing pending invocations
    WorkQueue _work_queue;

    std::atomic<bool> _ending{};

    static constexpr int MAX_EPOLL_EVENTS = 32;
    static constexpr int EPOLL_TIMEOUT = 1000;
  };

}

#endif
