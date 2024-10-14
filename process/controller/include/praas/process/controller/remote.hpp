#ifndef PRAAS_PROCESS_CONTROLLER_REMOTE_HPP
#define PRAAS_PROCESS_CONTROLLER_REMOTE_HPP

#include <praas/common/messages.hpp>
#include <praas/process/controller/config.hpp>
#include <praas/process/runtime/internal/buffer.hpp>

#include <optional>
#include <string>
#include <unordered_map>

#include <spdlog/logger.h>
#include <trantor/net/EventLoopThread.h>
#include <trantor/net/TcpClient.h>
#include <trantor/net/TcpServer.h>

namespace praas::process {
  struct Controller;
} // namespace praas::process

namespace praas::process::remote {

  enum class RemoteType { DATA_PLANE, CONTROL_PLANE, PROCESS, LOCAL_FUNCTION };

  struct Server {

    Server() = default;
    Server(const Server&) = default;
    Server(Server&&) = delete;
    Server& operator=(const Server&) = default;
    Server& operator=(Server&&) = delete;
    virtual ~Server() = default;

    virtual void poll(std::optional<std::string> control_plane_address = std::nullopt) = 0;

    // Receive messages to be sent by the process controller.
    // Includes results of invocations and put requests.
    virtual void invocation_result(
        RemoteType source, std::optional<std::string_view> remote_process,
        std::string_view invocation_id, int return_code,
        runtime::internal::BufferAccessor<const char> payload
    ) = 0;

    virtual void put_message(
        std::string_view process_id, std::string_view name,
        runtime::internal::Buffer<char>&& payload
    ) = 0;

    virtual void swap_confirmation(
        size_t size_bytes, double time
    ) = 0;

    virtual void invocation_request(
        std::string_view process_id, std::string_view function_name, std::string_view invocation_id,
        runtime::internal::Buffer<char>&& payload
    ) = 0;
  };

  struct Connection {

    enum class Status { DISCONNECTED, CONNECTING, CONNECTED };

    Status status = Status::DISCONNECTED;

    RemoteType type;

    std::optional<std::string> id;

    // std::weak_ptr<trantor::TcpConnection> conn;
    trantor::TcpConnectionPtr conn{};

    std::string ip_address{};

    int port{};

    common::message::MessageData cur_msg;

    common::message::MessageVariants parsed_msg;

    std::size_t bytes_to_read{};

    std::shared_ptr<trantor::TcpClient> client{};

    // FIXME: optimize - we need one message type
    // std::vector<
    //    std::tuple<std::unique_ptr<common::message::Message>, runtime::internal::Buffer<char>>>
    //    pendings_msgs;
    std::vector<std::tuple<common::message::MessageData, runtime::internal::Buffer<char>>>
        pendings_msgs;
  };

  struct TCPServer : Server {

    TCPServer(Controller&, const config::Controller&);

    virtual ~TCPServer();

    int port() const
    {
      return _server.address().toPort();
    }

    void invocation_result(
        RemoteType source, std::optional<std::string_view> remote_process,
        std::string_view invocation_id, int return_code,
        runtime::internal::BufferAccessor<const char> payload
    ) override;

    void put_message(
        std::string_view process_id, std::string_view name,
        runtime::internal::Buffer<char>&& payload
    ) override;

    void invocation_request(
        std::string_view process_id, std::string_view function_name, std::string_view invocation_id,
        runtime::internal::Buffer<char>&& payload
    );

    void swap_confirmation(size_t size_bytes, double time) override;

    void shutdown();

    void poll(std::optional<std::string> control_plane_address = std::nullopt);

  private:
    void _connect(Connection* conn);

    bool _handle_connection(
        const trantor::TcpConnectionPtr& connectionPtr, common::message::ProcessConnectionPtr msg
    );

    bool _handle_app_update(
        const trantor::TcpConnectionPtr& connectionPtr, common::message::ApplicationUpdatePtr msg
    );

    bool _handle_invocation(
        Connection& connection, common::message::InvocationRequestPtr msg,
        trantor::MsgBuffer* buffer
    );

    bool _handle_invocation_result(
        Connection& connection, common::message::InvocationResultPtr msg, trantor::MsgBuffer* buffer
    );

    bool _handle_put_message(
        Connection& connection, common::message::PutMessagePtr msg, trantor::MsgBuffer* buffer
    );

    void
    _handle_message(const trantor::TcpConnectionPtr& connectionPtr, trantor::MsgBuffer* buffer);

    std::atomic<bool> _is_running{};

    std::shared_ptr<spdlog::logger> _logger;

    // Main controller object
    Controller& _controller;

    // Main event loop thread
    trantor::EventLoopThread _loop_thread;

    // TCP server instance
    trantor::TcpServer _server;

    runtime::internal::BufferQueue<char> _buffers;

    // lock
    std::mutex _conn_mutex;
    std::unordered_map<std::string, std::shared_ptr<Connection>> _connection_data;
    std::shared_ptr<Connection> _data_plane;
    std::shared_ptr<Connection> _control_plane;
    std::shared_ptr<trantor::TcpClient> _control_plane_conn;

    static constexpr std::string_view DATAPLANE_ID = "DATA_PLANE";
    static constexpr std::string_view CONTROLPLANE_ID = "CONTROL_PLANE";
  };

} // namespace praas::process::remote

#endif
