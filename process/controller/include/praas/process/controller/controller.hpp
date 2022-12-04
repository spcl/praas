#ifndef PRAAS_PROCESS_CONTROLLER_HPP
#define PRAAS_PROCESS_CONTROLLER_HPP

#include <praas/process/controller/config.hpp>
#include <praas/process/controller/remote.hpp>
#include <praas/process/controller/workers.hpp>
#include <praas/process/runtime/ipc/ipc.hpp>
#include <praas/process/controller/messages.hpp>

#include <praas/common/messages.hpp>

#include <memory>
#include <string>
#include <variant>

#include <oneapi/tbb/concurrent_queue.h>

namespace praas::process::remote {

  struct Server;

}

namespace praas::process {

  struct Controller {

    struct ExternalMessage {
      remote::RemoteType source_type;
      // No value? Source is data or control plane.
      // Value? Process id.
      std::optional<std::string> source;
      praas::common::message::Message msg;
      runtime::Buffer<char> payload;
    };

    // State

    // Swapper object

    Controller(config::Controller cfg);

    ~Controller();

    void set_remote(remote::Server* server);

    void poll();

    void start();

    void shutdown();

    void shutdown_channels();

    // FIXME: this is only required because of the split between message and parsed message
    // There should be one type only!
    void remote_message(praas::common::message::Message &&, runtime::Buffer<char> &&, std::string process_id);

    void dataplane_message(praas::common::message::Message &&, runtime::Buffer<char> &&);

    void controlplane_message(praas::common::message::Message &&, runtime::Buffer<char> &&);

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
    std::deque<ExternalMessage> _external_queue;
    std::mutex _deque_lock;
    // No deque in tbb
    // https://community.intel.com/t5/Intel-oneAPI-Threading-Building/Is-there-a-concurrent-dequeue/m-p/873829
    //oneapi::tbb::concurrent_queue<ExternalMessage> _external_queue;

    // Queue storing pending invocations
    WorkQueue _work_queue;

    // TCP Server handling remote messages
    remote::Server* _server{};

    // Pending messages
    message::MessageStore _mailbox;

    message::PendingMessages _pending_msgs;

    std::atomic<bool> _ending{};

    std::string _process_id;

    static constexpr int MAX_EPOLL_EVENTS = 32;
    static constexpr int EPOLL_TIMEOUT = 100;

    static constexpr std::string_view SELF_PROCESS = "SELF";
  };

  extern Controller* INSTANCE;

  void set_terminate(Controller* controller);
  void set_signals();

}

#endif
