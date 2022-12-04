#ifndef PRAAS_PROCESS_CONTROLLER_REMOTE_HPP
#define PRAAS_PROCESS_CONTROLLER_REMOTE_HPP

#include <praas/process/controller/config.hpp>
#include <praas/process/runtime/buffer.hpp>

#include <optional>
#include <string>
#include <unordered_map>

#include <trantor/net/EventLoopThread.h>
#include <trantor/net/TcpServer.h>
#include "praas/common/messages.hpp"

namespace praas::process {
  struct Controller;
}

namespace praas::process::remote {

  enum class RemoteType {
    DATA_PLANE,
    CONTROL_PLANE,
    PROCESS,
    LOCAL_FUNCTION
  };

  struct Server {

    Server() = default;
    Server(const Server&) = default;
    Server(Server&&) = delete;
    Server& operator=(const Server&) = default;
    Server& operator=(Server&&) = delete;
    virtual ~Server() = default;

    virtual void poll() = 0;

    // Receive messages to be sent by the process controller.
    // Includes results of invocations and put requests.
    virtual void invocation_result(
        RemoteType source,
        std::optional<std::string_view> remote_process,
        std::string_view invocation_id,
        int return_code,
        runtime::Buffer<char> && payload
    ) = 0;
    virtual void put_message() = 0;

  };

  struct Connection {

    RemoteType type;

    std::optional<std::string> id;

    //std::weak_ptr<trantor::TcpConnection> conn;
    trantor::TcpConnectionPtr conn;

    common::message::Message cur_msg;

    std::size_t bytes_to_read{};

  };

  struct TCPServer : Server {

    TCPServer(Controller&, const config::Controller&);

    virtual ~TCPServer();

    int port() const
    {
      return _server.address().toPort();
    }

    void invocation_result(
        RemoteType source,
        std::optional<std::string_view> remote_process,
        std::string_view invocation_id,
        int return_code,
        runtime::Buffer<char> && payload
    ) override;

    void put_message() override;

    void shutdown();

    void poll() override;

  private:

    bool _handle_connection(const trantor::TcpConnectionPtr& connectionPtr, const common::message::ProcessConnectionParsed& msg);

    bool _handle_invocation(Connection& connection,
      const common::message::InvocationRequestParsed& msg, trantor::MsgBuffer* buffer);

    void _handle_message(const trantor::TcpConnectionPtr& connectionPtr, trantor::MsgBuffer* buffer);

    std::atomic<bool> _is_running{};

    // Main controller object
    Controller& _controller;

    // Main event loop thread
    trantor::EventLoopThread _loop_thread;

    // TCP server instance
    trantor::TcpServer _server;

    runtime::BufferQueue<char> _buffers;

    // lock
    std::mutex _data_mock;
    std::unordered_map<std::string, std::shared_ptr<Connection>> _connection_data;
    std::shared_ptr<Connection> _data_plane;
    std::shared_ptr<Connection> _control_plane;

    static constexpr std::string_view DATAPLANE_ID = "DATA_PLANE";
    static constexpr std::string_view CONTROLPLANE_ID = "CONTROL_PLANE";
  };

} // namespace praas::process::remote

#endif
