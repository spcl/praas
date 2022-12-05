#include <praas/process/controller/controller.hpp>
#include <praas/process/controller/remote.hpp>
#include <praas/common/messages.hpp>
#include <spdlog/spdlog.h>
#include <variant>
#include "praas/common/application.hpp"

namespace praas::process::remote {

  TCPServer::TCPServer(Controller& controller, const config::Controller& cfg):
    _is_running(true),
    _controller(controller),
    _server(_loop_thread.getLoop(), trantor::InetAddress(cfg.port), "tcpserver")
  {
    // FIXME: do I need to configure this?
    _server.setIoLoopNum(1);

    _server.setConnectionCallback(
        [](const trantor::TcpConnectionPtr& connectionPtr) {
          if(connectionPtr->connected()) {
            spdlog::info("New connection from {}", connectionPtr->peerAddr().toIpPort());
          } else {
            spdlog::info("Disconnected from {}", connectionPtr->peerAddr().toIpPort());
          }
        }
    );

    _server.setRecvMessageCallback(
        [this](const trantor::TcpConnectionPtr& connectionPtr, trantor::MsgBuffer* buffer) {
          _handle_message(connectionPtr, buffer);
        }
    );

    _loop_thread.getLoop()->runOnQuit({
      []() {
      spdlog::info("Server is quitting");

        const std::exception_ptr &eptr = std::current_exception();
        if(eptr) {
          try { std::rethrow_exception(eptr); }
          catch (const std::exception &e) { spdlog::error("Exception thrown {}", e.what()); }
          catch (const std::string    &e) { spdlog::error("Exception thrown {}", e); }
          catch (const char           *e) { spdlog::error("Exception thrown {}", e); }
          catch (...)                     { spdlog::error("Exception thrown, unknown!"); }
        }
      }
    });
  }

  TCPServer::~TCPServer()
  {
    if (_is_running) {
      shutdown();
    }
  }

  void TCPServer::shutdown()
  {
    _server.stop();
    _loop_thread.getLoop()->quit();
    _loop_thread.wait();

    _is_running = false;
  }

  void TCPServer::poll()
  {
    spdlog::info("TCP server is starting!");
    _loop_thread.run();
    _server.start();
  }

  /**
   *
   * Receives:
   * - invocation requests + data (user, process)
   * - FIXME put + data (process)
   *
   * From control plane:
   * - FIXME updates to the process
   * - FIXME data plane statistics
   *
   **/
  void TCPServer::_handle_message(const trantor::TcpConnectionPtr& connectionPtr, trantor::MsgBuffer* buffer)
  {
    auto conn = connectionPtr->getContext<Connection>();

    spdlog::info("Received {} bytes {}", buffer->readableBytes(), conn != nullptr);

    // Registration of the connection
    if(!conn) {

      if(buffer->readableBytes() < praas::common::message::Message::BUF_SIZE) {
        return;
      }

      auto msg = praas::common::message::Message::parse_message(buffer->peek());

      std::cerr << "test" << std::endl;
      if(std::holds_alternative<common::message::ProcessConnectionParsed>(msg)) {
        _handle_connection(connectionPtr, std::get<common::message::ProcessConnectionParsed>(msg));
      } else {
        spdlog::error("Ignoring message from an unknown receipient");
      }
      buffer->retrieve(praas::common::message::Message::BUF_SIZE);

      if(buffer->readableBytes() > 0) {
        std::cerr << "receive again" << std::endl;
        _handle_message(connectionPtr, buffer);
      }
      return;
    }

    bool consumed = false;

    spdlog::info("received {} {}", conn->bytes_to_read, buffer->readableBytes());
    // Checking if we have enough bytes
    if(conn->bytes_to_read > 0) {

      if(buffer->readableBytes() < conn->bytes_to_read)
        return;

      // Three types of messages require long payloads:
      // invoke
      // put
      auto msg = conn->cur_msg.parse();
      consumed = std::visit(
          common::message::overloaded{
            [this, buffer, conn = conn.get()](
                common::message::InvocationRequestParsed & invoc
            ) mutable -> bool {
              return _handle_invocation(
                  *conn,
                  invoc,
                  buffer
              );
            },
            [this, buffer, conn = conn.get()](
              common::message::PutMessageParsed & req
            ) mutable -> bool {
              return _handle_put_message(
                *conn,
                req,
                buffer
              );
            },
            [](auto &) mutable -> bool {
              return false;
            }
          },
          // FIXME: we should store an already parsed variant
          msg
      );

      return;
    } else {

      if(buffer->readableBytes() < praas::common::message::Message::BUF_SIZE)
        return;

      // FIXME: improve message handling - one variant type
      auto msg = praas::common::message::Message::parse_message(buffer->peek());

      // FIXME: process update (list of processes)
      // FIXME: put request
      // FIXME: swap request
      consumed = std::visit(
          common::message::overloaded{
            [this, buffer, conn = conn.get()](
              common::message::InvocationRequestParsed & invoc
            ) mutable -> bool {
              return _handle_invocation(
                  *conn,
                  invoc,
                  buffer
              );
            },
            [this, buffer, conn = conn.get()](common::message::PutMessageParsed& req) mutable -> bool {
              return _handle_put_message(
                *conn,
                req,
                buffer
              );
            },
            [this, connectionPtr, buffer](common::message::ProcessConnectionParsed& msg) mutable -> bool {
              // Connection always consumed a message
              _handle_connection(connectionPtr, msg);
              buffer->retrieve(praas::common::message::Message::BUF_SIZE);
              return true;
            },
            [this, connectionPtr, buffer](common::message::ApplicationUpdateParsed& msg) mutable -> bool {
              _handle_app_update(connectionPtr, msg);
              buffer->retrieve(praas::common::message::Message::BUF_SIZE);
              return true;
            },
            [buffer,msg](auto &) mutable -> bool {
              spdlog::error("Unsupported message type!");
              buffer->retrieve(praas::common::message::Message::BUF_SIZE);
              return true;
            }},
            msg
      );

    }

    std::cerr << consumed << " " << buffer->readableBytes() << std::endl;
    // Check if there is more data to be read
    if(consumed && buffer->readableBytes() > 0) {
      _handle_message(connectionPtr, buffer);
    }

  }

  bool TCPServer::_handle_invocation(Connection& connection,
      const common::message::InvocationRequestParsed& msg, trantor::MsgBuffer* buffer)
  {
    // We just started
    if(connection.bytes_to_read == 0) {

      connection.bytes_to_read = msg.payload_size();
      spdlog::info("Invocation start {}",connection.bytes_to_read);
      common::message::InvocationRequest req;
      req.invocation_id(msg.invocation_id());
      req.function_name(msg.function_name());
      req.payload_size(msg.payload_size());
      // FIXME: avoid copy here - just store the actual variant
      connection.cur_msg = std::move(req);

      buffer->retrieve(praas::common::message::Message::BUF_SIZE);
    }

    // Check that we have the payload
    if(buffer->readableBytes() >= msg.payload_size()) {

      auto buf = _buffers.retrieve_buffer(msg.payload_size());
      std::copy_n(buffer->peek(), msg.payload_size(), buf.data());
      buf.len = msg.payload_size();
      buffer->retrieve(msg.payload_size());

      spdlog::info(
          "Received complete invocation request of {}, with {} bytes of input",
          msg.function_name(),
          msg.payload_size()
      );
      if(connection.type == RemoteType::DATA_PLANE) {
        _controller.dataplane_message(std::move(connection.cur_msg), std::move(buf));
      } else if(connection.type == RemoteType::CONTROL_PLANE) {
        _controller.controlplane_message(std::move(connection.cur_msg), std::move(buf));
      } else {
        _controller.remote_message(std::move(connection.cur_msg), std::move(buf), connection.id.value());
      }

      connection.bytes_to_read = 0;

      return true;
    }
    // Not enough payload, not consumed
    else {
      return false;
    }
  }

  bool TCPServer::_handle_put_message(Connection& connection,
      const common::message::PutMessageParsed& msg, trantor::MsgBuffer* buffer)
  {
    // We just started
    if(connection.bytes_to_read == 0) {

      connection.bytes_to_read = msg.total_length();
      spdlog::info("Start receiving message of size {}", connection.bytes_to_read);
      // FIXME: avoid copy here - just store the actual variant
      common::message::PutMessage req;
      req.name(msg.name());
      connection.cur_msg = std::move(req);

      buffer->retrieve(praas::common::message::Message::BUF_SIZE);
    }

    // Check that we have the payload
    if(buffer->readableBytes() >= connection.bytes_to_read) {

      auto buf = _buffers.retrieve_buffer(connection.bytes_to_read);
      std::copy_n(buffer->peek(), connection.bytes_to_read, buf.data());
      buf.len = connection.bytes_to_read;
      buffer->retrieve(connection.bytes_to_read);

      spdlog::info(
          "Received complete put messsage of {}, with {} bytes of input",
          msg.name(),
          connection.bytes_to_read
      );

      _controller.remote_message(std::move(connection.cur_msg), std::move(buf), connection.id.value());

      connection.bytes_to_read = 0;

      return true;
    }
    // Not enough payload, not consumed
    return false;
  }

  bool TCPServer::_handle_connection(const trantor::TcpConnectionPtr& connectionPtr, const common::message::ProcessConnectionParsed& msg)
  {
    std::shared_ptr<Connection> conn;

    auto find_iter = _connection_data.find(std::string{msg.process_name()});
    if(find_iter != _connection_data.end()) {

      // Update connection
      (*find_iter).second->conn = connectionPtr;
      if(msg.process_name() == DATAPLANE_ID) {
        _data_plane = (*find_iter).second;
      } else if(msg.process_name() == CONTROLPLANE_ID) {
        _control_plane = (*find_iter).second;
      }

      spdlog::info("Registered new remote connection for an existing IP data");

      connectionPtr->setContext((*find_iter).second);

    } else {

      if(msg.process_name() == DATAPLANE_ID) {
        conn = std::make_shared<Connection>(RemoteType::DATA_PLANE, std::nullopt, connectionPtr);
        _data_plane = conn;
      } else if(msg.process_name() == CONTROLPLANE_ID) {
        conn = std::make_shared<Connection>(RemoteType::CONTROL_PLANE, std::nullopt, connectionPtr);
        _control_plane = conn;
      } else {
        conn = std::make_shared<Connection>(RemoteType::PROCESS, std::string{msg.process_name()}, connectionPtr);
      }

      spdlog::info("Registered new remote connection");

      auto [iter, inserted] = _connection_data.emplace(msg.process_name(), std::move(conn));
      // FIXME: handle insertion failure
      connectionPtr->setContext(iter->second);

    }

    praas::common::message::ProcessConnection req;
    req.process_name("CORRECT");
    connectionPtr->send(req.bytes(), req.BUF_SIZE);

    return true;
  }

  void TCPServer::invocation_result(
      RemoteType source,
      std::optional<std::string_view> remote_process,
      std::string_view invocation_id,
      int return_code,
      runtime::Buffer<char> && payload
  )
  {
    Connection* conn = nullptr;
    if(source == RemoteType::DATA_PLANE) {
      conn = _data_plane.get();
    } else if(source == RemoteType::CONTROL_PLANE) {
      conn = _control_plane.get();
    } else if(remote_process.has_value()) {

      auto it = _connection_data.find(std::string{remote_process.value()});
      if(it != _connection_data.end()) {
        conn = (*it).second.get();
      }
    }
    if(!conn) {
      spdlog::error("Ignoring invocation result of {} for unknown recipient!", invocation_id);
      return;
    }

    praas::common::message::InvocationResult req;
    req.invocation_id(invocation_id);
    // FIXME: eliminate that
    req.return_code(return_code);
    req.total_length(payload.len);

    conn->conn->send(req.bytes(), req.BUF_SIZE);
    if(payload.len > 0) {
      conn->conn->send(payload.data(), payload.len);
    }
  }

  bool TCPServer::_handle_app_update(
      const trantor::TcpConnectionPtr& connectionPtr,
      const common::message::ApplicationUpdateParsed& msg)
  {
    _controller.update_application(
      static_cast<common::Application::Status>(msg.status_change()),
      msg.process_id()
    );

    std::string process_id{msg.process_id()};
    auto iter = _connection_data.find(process_id);
    if(iter != _connection_data.end()) {
      (*iter).second->ip_address = msg.ip_address();
      (*iter).second->port = msg.port();
    } else {

      _connection_data.emplace(
        process_id,
        std::move(
          std::make_shared<Connection>(
            RemoteType::PROCESS, process_id, nullptr, std::string{msg.ip_address()}, msg.port()
          )
        )
      );

    }

    return true;
  }

  void TCPServer::put_message(std::string_view process_id, std::string_view name, runtime::Buffer<char> && payload)
  {
    auto iter = _connection_data.find(std::string{process_id});
    if(iter == _connection_data.end()) {
      // FIXME: return error?
      spdlog::error("Sending message to an unknown process {}!", process_id);
      return;
    }

    Connection* conn = iter->second.get();

    if(!conn->conn) {
      spdlog::info("Establish connection");

      // FIXME: is it only for put message?
      auto put_req = std::make_unique<praas::common::message::PutMessage>();
      put_req->name(name);
      put_req->process_id(_controller.process_id());
      put_req->total_length(payload.len);
      conn->pending_msg = std::move(put_req);
      conn->pending_payload = std::move(payload);
      spdlog::info("Store message of size {}", conn->pending_payload.len);

      _connect(conn, name);

    } else {
      spdlog::info("Communicate");
      abort();
      // FIXME: send message
      conn->conn->send(payload.data(), payload.len);
    }
  }

  void TCPServer::_connect(Connection * conn, std::string_view name)
  {

    conn->client = std::make_shared<trantor::TcpClient>(
      this->_server.getLoop(),
      trantor::InetAddress{conn->ip_address, static_cast<uint16_t>(conn->port)},
      "client"
    );

    conn->client->setConnectionCallback(
      [this, connection = conn](const trantor::TcpConnectionPtr &conn) -> void {
          if (conn->connected()) {

            spdlog::info("Connected!");
            praas::common::message::ProcessConnection req;
            req.process_name(_controller.process_id());
            conn->send(req.bytes(), req.BUF_SIZE);

            connection->conn = conn;

            if(connection->pending_msg) {

              auto* msg = connection->pending_msg.get();
              spdlog::info("Send message {} {}", connection->pending_payload.len, msg->total_length());
              conn->send(msg->bytes(), msg->BUF_SIZE);
              if(connection->pending_payload.len > 0) {
                conn->send(connection->pending_payload.data(), connection->pending_payload.len);
              }

            }

          } else {
            spdlog::info("Process disconnected");
          }
      });

    conn->client->connect();
  }

}
