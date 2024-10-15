#ifndef PRAAS_PROCESS_CONTROLLER_HPP
#define PRAAS_PROCESS_CONTROLLER_HPP

#include <praas/common/application.hpp>
#include <praas/common/messages.hpp>
#include <praas/process/controller/config.hpp>
#include <praas/process/controller/messages.hpp>
#include <praas/process/controller/remote.hpp>
#include <praas/process/controller/workers.hpp>
#include <praas/process/runtime/internal/ipc/ipc.hpp>

#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <variant>

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
      praas::common::message::MessageData msg;
      runtime::internal::Buffer<char> payload;
    };

    Controller(config::Controller cfg);

    ~Controller();

    void set_remote(remote::Server* server);

    void poll();

    void start();

    void shutdown();

    void shutdown_channels();

    bool swap_in(const std::string& location);

    void swap_out(const std::string& location);

    // FIXME: this is only required because of the split between message and parsed message
    // There should be one type only!
    void remote_message(
        praas::common::message::MessageData&&, runtime::internal::Buffer<char>&&,
        std::string process_id
    );

    void
    dataplane_message(praas::common::message::MessageData&&, runtime::internal::Buffer<char>&&);

    void
    controlplane_message(praas::common::message::MessageData&&, runtime::internal::Buffer<char>&&);

    void update_application(common::Application::Status status, std::string_view process);

    std::string_view process_id() const
    {
      return _process_id;
    }

  private:
    void _process_application_updates(const std::vector<common::ApplicationUpdate>& updates);
    void _process_external_message(ExternalMessage& msg);
    void
    _process_internal_message(FunctionWorker& worker, const runtime::internal::ipc::Message& msg, runtime::internal::Buffer<char>&&);

    // Store the message data, and check if there is a pending invocation waiting for this result
    void _process_put(
        const runtime::internal::ipc::PutRequestParsed& req,
        runtime::internal::Buffer<char>&& payload
    );

    // Check if there is a message with this data. If yes, then respond immediately.
    // If not, then put in the structure for pending messages.
    void _process_get(runtime::internal::Buffer<char>&&);
    void
    _process_get(const runtime::internal::ipc::Message& msg, runtime::internal::Buffer<char>&&);

    // Put the invocation into a work queue.
    // Store the invocation on a list of pending invocations.
    // void _process_invocation(runtime::Buffer<char> &&);
    void
    _process_invocation(FunctionWorker& worker, const runtime::internal::ipc::InvocationRequestParsed& msg, runtime::internal::Buffer<char>&&);

    void _process_invocation_result(
        FunctionWorker& worker, std::string_view invocation_id, int return_code,
        runtime::internal::BufferAccessor<const char> payload
    );

    void _process_invocation_result(
        const InvocationSource& source, std::string_view invocation_id, int return_code,
        runtime::internal::BufferAccessor<const char> payload
    );

    // Retrieve the pending invocation object.
    // Forward the response to the owner.
    void _process_result(runtime::internal::Buffer<char>&&);
    void
    _process_result(const runtime::internal::ipc::Message& msg, runtime::internal::Buffer<char>&&);

    int _epoll_fd;

    int _event_fd;

    std::shared_ptr<spdlog::logger> _logger;

    static constexpr int DEFAULT_BUFFER_MESSAGES = 20;
    static constexpr int DEFAULT_BUFFER_SIZE = 5 * 1024 * 1024;

    runtime::internal::BufferQueue<char> _buffers;

    // Function triggers
    runtime::internal::Functions _functions;

    // Function workers (IPC) - seperate processes.
    Workers _workers;

    // Queue storing external data provided by the TCP server
    std::deque<ExternalMessage> _external_queue;
    std::mutex _deque_lock;
    // No deque in tbb
    // https://community.intel.com/t5/Intel-oneAPI-Threading-Building/Is-there-a-concurrent-dequeue/m-p/873829
    // oneapi::tbb::concurrent_queue<ExternalMessage> _external_queue;

    // Current world
    std::mutex _app_lock;
    common::Application _application;
    std::deque<common::ApplicationUpdate> _app_updates;

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
    static constexpr int EPOLL_TIMEOUT = 1000;

    static constexpr std::string_view SELF_PROCESS = "SELF";
  };

  extern Controller* INSTANCE;

  void set_terminate(Controller* controller);
  void set_signals();

} // namespace praas::process

#endif
