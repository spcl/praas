#ifndef PRAAS_PROCESS_CONTROLLER_HPP
#define PRAAS_PROCESS_CONTROLLER_HPP

#include <praas/process/controller/config.hpp>
#include <praas/process/controller/workers.hpp>
#include <praas/process/runtime/ipc/ipc.hpp>
#include <praas/common/messages.hpp>

#include <memory>
#include <string>

#include <oneapi/tbb/concurrent_queue.h>

namespace praas::process::remote {

  struct Server;

}

namespace praas::process {


  struct Controller {

    struct ExternalMessage {
      // No value? Source is data plane.
      // Value? Process id.
      std::optional<std::string> source;
      praas::common::message::Message msg;
      runtime::Buffer<char> payload;
    };

    // State

    // Messages

    // Func queue

    // Swapper object

    // TCP Poller object

    // lock-free queue from TCP server
    // lock-free queue to TCP server

    Controller(config::Controller cfg);

    ~Controller();

    void set_remote(remote::Server* server);

    void poll();

    void start();

    void shutdown();

    void remote_message(praas::common::message::Message &&, runtime::Buffer<char> &&, std::string process_id);

    void dataplane_message(praas::common::message::Message &&, runtime::Buffer<char> &&);

  private:

    void _process_external_message(ExternalMessage & msg);
    void _process_internal_message(FunctionWorker & worker, const runtime::ipc::Message & msg, runtime::Buffer<char> &&);

    // Store the message data, and check if there is a pending invocation waiting for this result
    void _process_put(runtime::Buffer<char> &&);
    void _process_put(const runtime::ipc::Message & msg, runtime::Buffer<char> &&);

    // Check if there is a message with this data. If yes, then respond immediately.
    // If not, then put in the structure for pending messages.
    void _process_get(runtime::Buffer<char> &&);
    void _process_get(const runtime::ipc::Message & msg, runtime::Buffer<char> &&);

    // Put the invocation into a work queue.
    // Store the invocation on a list of pending invocations.
    void _process_invocation(runtime::Buffer<char> &&);
    void _process_invocation(const runtime::ipc::Message & msg, runtime::Buffer<char> &&);

    // Retrieve the pending invocation object.
    // Forward the response to the owner.
    void _process_result(runtime::Buffer<char> &&);
    void _process_result(const runtime::ipc::Message & msg, runtime::Buffer<char> &&);

    int _epoll_fd;

    int _event_fd;

    static constexpr int DEFAULT_BUFFER_MESSAGES = 20;
    static constexpr int DEFAULT_BUFFER_SIZE = 512 * 1024 * 1024;

    runtime::BufferQueue<char> _buffers;

    // Function triggers
    runtime::functions::Functions _functions;

    // Function workers (IPC) - seperate processes.
    Workers _workers;

    // Queue storing external data provided by the TCP server
    oneapi::tbb::concurrent_queue<ExternalMessage> _external_queue;

    // Queue storing pending invocations
    WorkQueue _work_queue;

    // Server handling remote messages
    remote::Server* _server{};

    std::atomic<bool> _ending{};

    static constexpr int MAX_EPOLL_EVENTS = 32;
    static constexpr int EPOLL_TIMEOUT = 1000;
  };

}

#endif
