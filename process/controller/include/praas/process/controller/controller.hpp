#ifndef PRAAS_PROCESS_CONTROLLER_HPP
#define PRAAS_PROCESS_CONTROLLER_HPP

#include <praas/process/controller/config.hpp>
#include <praas/process/controller/workers.hpp>
#include <praas/process/runtime/ipc/ipc.hpp>
#include <praas/common/messages.hpp>

#include <memory>

#include <oneapi/tbb/concurrent_queue.h>

namespace praas::process {


  struct Controller {

    struct ExternalMessage {
      praas::common::message::Message msg;
      runtime::Buffer<char> payload;
    };

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

    void wakeup(praas::common::message::Message &&, runtime::Buffer<char> &&);

  private:

    void _process_external_message(ExternalMessage & msg);
    void _process_internal_message(const runtime::ipc::Message & msg, runtime::Buffer<char>);

    int _epoll_fd;

    int _event_fd;

    static constexpr int DEFAULT_BUFFER_MESSAGES = 20;
    static constexpr int DEFAULT_BUFFER_SIZE = 512 * 1024 * 1024;

    runtime::BufferQueue<char> _buffers;

    // Function triggers
    runtime::functions::Functions _functions;

    Workers _workers;

    // Queue storing external data provided by the TCP server
    oneapi::tbb::concurrent_queue<ExternalMessage> _external_queue;

    // Queue storing pending invocations
    WorkQueue _work_queue;

    std::atomic<bool> _ending{};

    static constexpr int MAX_EPOLL_EVENTS = 32;
    static constexpr int EPOLL_TIMEOUT = 1000;
  };

}

#endif
