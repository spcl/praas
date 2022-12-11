#include <praas/process/controller/remote.hpp>

#include <praas/common/application.hpp>
#include <praas/common/util.hpp>
#include <praas/process/controller/controller.hpp>
#include <praas/common/messages.hpp>

#include <variant>

#include <spdlog/spdlog.h>
#include <trantor/utils/MsgBuffer.h>

namespace praas::process::remote {

  TCPServer::TCPServer(Controller& controller, const config::Controller& cfg):
    _is_running(true),
    _controller(controller),
    _server(_loop_thread.getLoop(), trantor::InetAddress(cfg.port), "tcpserver")
  {
    // FIXME: do I need to configure this?
    _server.setIoLoopNum(1);

    _server.setConnectionCallback(
        [this](const trantor::TcpConnectionPtr& connectionPtr) {
          if(connectionPtr->connected()) {
            SPDLOG_LOGGER_DEBUG(_logger, "New connection from {}", connectionPtr->peerAddr().toIpPort());
            connectionPtr->setTcpNoDelay(true);
          } else {
            SPDLOG_LOGGER_DEBUG(_logger, "Disconnected from {}", connectionPtr->peerAddr().toIpPort());
          }
        }
    );

    _server.setRecvMessageCallback(
        [this](const trantor::TcpConnectionPtr& connectionPtr, trantor::MsgBuffer* buffer) {
          _handle_message(connectionPtr, buffer);
        }
    );

    _loop_thread.getLoop()->runOnQuit({
      [this]() {
        _logger->info("Server is quitting");

        const std::exception_ptr &eptr = std::current_exception();
        if(eptr) {
          try { std::rethrow_exception(eptr); }
          catch (const std::exception &e) { _logger->error("Exception thrown {}", e.what()); }
          catch (const std::string    &e) { _logger->error("Exception thrown {}", e); }
          catch (const char           *e) { _logger->error("Exception thrown {}", e); }
          catch (...)                     { _logger->error("Exception thrown, unknown!"); }
        }
      }
    });

    _logger = common::util::create_logger("TCPServer");
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

  void TCPServer::poll(std::optional<std::string> control_plane_address)
  {
    _logger->info("TCP server is starting!");
    _loop_thread.run();
    _server.start();

    if(!control_plane_address.has_value()) {
      return;
    }

    // FIXME: make a generic connection method
    auto pos = control_plane_address.value().find(':');
    std::string address = control_plane_address.value().substr(0, pos);
    int port = std::stoi(control_plane_address.value().substr(pos + 1, std::string::npos));

    auto conn = std::make_shared<Connection>(
      Connection::Status::CONNECTING,
      RemoteType::CONTROL_PLANE,
      std::nullopt,
      nullptr
    );

    _control_plane = conn;
    auto [iter, inserted] = _connection_data.emplace("CONTROLPLANE", std::move(conn));
    common::util::assert_true(inserted);

    (*iter).second->client = std::make_shared<trantor::TcpClient>(
      this->_server.getLoop(),
      trantor::InetAddress{address, static_cast<uint16_t>(port)},
      "client"
    );

    std::promise<void> connected;
    (*iter).second->client->setConnectionCallback(
      [this, iter, &connected](const trantor::TcpConnectionPtr& connectionPtr) -> void {

        if (connectionPtr->connected()) {

          // Send my name
          _logger->info("Connected to control plane, sending my registration");
          praas::common::message::ProcessConnection req;
          req.process_name(_controller.process_id());
          connectionPtr->send(req.bytes(), req.BUF_SIZE);

          // FIXME: make it configurable
          connectionPtr->setTcpNoDelay(true);

          connectionPtr->setContext((*iter).second);

          (*iter).second->conn = connectionPtr;

          connected.set_value();

        } else {
          _logger->error("Terminated connection to control plane!");
          _control_plane.reset();
        }
      });

    (*iter).second->client->setMessageCallback(
      [this](const trantor::TcpConnectionPtr &conn, trantor::MsgBuffer* buffer) -> void {
        _logger->info("Control plane message");
        _handle_message(conn, buffer);
      }
    );

    _logger->info("Establishing connection to control plane at {}:{}", address, port);
    (*iter).second->client->connect();
    // FIXME: timed wait
    connected.get_future().wait();
    _logger->info("Finished setting up control plane connection {}:{}", address, port);
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

    SPDLOG_LOGGER_DEBUG(_logger, "Received {} bytes active connection? {}", buffer->readableBytes(), conn != nullptr);

    // Registration of the connection
    if(!conn) {

      if(buffer->readableBytes() < praas::common::message::Message::BUF_SIZE) {
        return;
      }

      auto msg = praas::common::message::Message::parse_message(buffer->peek());

      if(std::holds_alternative<common::message::ProcessConnectionParsed>(msg)) {
        _handle_connection(connectionPtr, std::get<common::message::ProcessConnectionParsed>(msg));
      } else {
        _logger->error("Ignoring message from an unknown receipient");
      }
      buffer->retrieve(praas::common::message::Message::BUF_SIZE);

      if(buffer->readableBytes() > 0) {
        _handle_message(connectionPtr, buffer);
      }
      return;
    }

    bool consumed = false;

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
            [this, buffer, conn = conn.get()](
              common::message::InvocationResultParsed & invoc
            ) mutable -> bool {
              return _handle_invocation_result(
                  *conn,
                  invoc,
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

    } else {

      if(buffer->readableBytes() < praas::common::message::Message::BUF_SIZE)
        return;

      // FIXME: improve message handling - one variant type
      auto msg = praas::common::message::Message::parse_message(buffer->peek());

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
            [this, buffer, conn = conn.get()](
              common::message::InvocationResultParsed & invoc
            ) mutable -> bool {
              return _handle_invocation_result(
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
              SPDLOG_LOGGER_DEBUG(_logger, "Confirmation of registration {}", msg.process_name());
              buffer->retrieve(praas::common::message::Message::BUF_SIZE);
              return true;
            },
            [this, connectionPtr, buffer](common::message::ApplicationUpdateParsed& msg) mutable -> bool {
              _handle_app_update(connectionPtr, msg);
              buffer->retrieve(praas::common::message::Message::BUF_SIZE);
              return true;
            },
            [this, buffer, msg](auto &) mutable -> bool {
              _logger->error("Unsupported message type!");
              buffer->retrieve(praas::common::message::Message::BUF_SIZE);
              return true;
            }},
            msg
      );

    }

    SPDLOG_LOGGER_DEBUG(_logger, "Consumed message? {} There are {} bytes remaining", consumed, buffer->readableBytes());
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

      SPDLOG_LOGGER_DEBUG(_logger,
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

  bool TCPServer::_handle_invocation_result(Connection& connection,
      const common::message::InvocationResultParsed& msg, trantor::MsgBuffer* buffer)
  {
    // We just started
    if(connection.bytes_to_read == 0) {

      connection.bytes_to_read = msg.total_length();
      common::message::InvocationResult req;
      req.invocation_id(msg.invocation_id());
      req.return_code(msg.return_code());
      // FIXME: avoid copy here - just store the actual variant
      connection.cur_msg = std::move(req);

      buffer->retrieve(praas::common::message::Message::BUF_SIZE);
    }

    // Check that we have the payload
    if(buffer->readableBytes() >= connection.bytes_to_read) {

      auto buf = _buffers.retrieve_buffer(connection.bytes_to_read);
      std::copy_n(buffer->peek(), connection.bytes_to_read, buf.data());
      buf.len = connection.bytes_to_read;
      buffer->retrieve(connection.bytes_to_read);

      SPDLOG_LOGGER_DEBUG(_logger,
          "Received invocation result for id {}, with {} bytes of input",
          msg.invocation_id(),
          msg.total_length()
      );
      // FIXME: this can only come from remote process -throw some exception
      _controller.remote_message(std::move(connection.cur_msg), std::move(buf), connection.id.value());

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
      // FIXME: avoid copy here - just store the actual variant
      common::message::PutMessage req;
      req.name(msg.name());
      req.process_id(msg.process_id());
      connection.cur_msg = std::move(req);

      buffer->retrieve(praas::common::message::Message::BUF_SIZE);
    }

    // Check that we have the payload
    if(buffer->readableBytes() >= connection.bytes_to_read) {

      auto buf = _buffers.retrieve_buffer(connection.bytes_to_read);
      std::copy_n(buffer->peek(), connection.bytes_to_read, buf.data());
      buf.len = connection.bytes_to_read;
      buffer->retrieve(connection.bytes_to_read);

      _controller.remote_message(std::move(connection.cur_msg), std::move(buf), connection.id.value());

      connection.bytes_to_read = 0;

      SPDLOG_LOGGER_DEBUG(
        _logger,
        "Finished processing PUT, {} remaining bytes",
        buffer->readableBytes()
      );
      return true;
    }
    // Not enough payload, not consumed
    return false;
  }

  bool TCPServer::_handle_connection(const trantor::TcpConnectionPtr& connectionPtr, const common::message::ProcessConnectionParsed& msg)
  {
    std::shared_ptr<Connection> conn;

    {

      std::unique_lock<std::mutex> lock{_conn_mutex};

      auto find_iter = _connection_data.find(std::string{msg.process_name()});
      if(find_iter != _connection_data.end()) {

        // Update connection
        (*find_iter).second->conn = connectionPtr;
        (*find_iter).second->status = Connection::Status::CONNECTED;
        if(msg.process_name() == DATAPLANE_ID) {
          _data_plane = (*find_iter).second;
        } else if(msg.process_name() == CONTROLPLANE_ID) {
          _control_plane = (*find_iter).second;
        }

        SPDLOG_LOGGER_DEBUG(_logger, "Registered new remote connection for an existing IP data");
        connectionPtr->setContext((*find_iter).second);

      } else {

        // FIXME: we no longer accept connect, we connect by ourselves
        if(msg.process_name() == DATAPLANE_ID) {
          conn = std::make_shared<Connection>(Connection::Status::CONNECTED, RemoteType::DATA_PLANE, std::nullopt, connectionPtr);
          _data_plane = conn;
        } else if(msg.process_name() == CONTROLPLANE_ID) {
          conn = std::make_shared<Connection>(Connection::Status::CONNECTED, RemoteType::CONTROL_PLANE, std::nullopt, connectionPtr);
          _control_plane = conn;
        } else {
          conn = std::make_shared<Connection>(Connection::Status::CONNECTED, RemoteType::PROCESS, std::string{msg.process_name()}, connectionPtr);

        }

        SPDLOG_LOGGER_DEBUG(_logger, "Registered new remote connection");

        auto [iter, inserted] = _connection_data.emplace(msg.process_name(), std::move(conn));
        // FIXME: handle insertion failure
        connectionPtr->setContext(iter->second);

      }

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
    {
      std::unique_lock<std::mutex> lock{_conn_mutex};

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
        _logger->error("Ignoring invocation result of {} for unknown recipient!", invocation_id);
        return;
      }
    }

    _logger->info("Submit invocation result of {}", invocation_id);
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
      const trantor::TcpConnectionPtr&,
      const common::message::ApplicationUpdateParsed& msg)
  {
    _controller.update_application(
      static_cast<common::Application::Status>(msg.status_change()),
      msg.process_id()
    );

    std::unique_lock<std::mutex> lock{_conn_mutex};
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
            Connection::Status::DISCONNECTED, RemoteType::PROCESS, process_id,
            nullptr, std::string{msg.ip_address()}, msg.port()
          )
        )
      );

    }

    return true;
  }

  void TCPServer::put_message(std::string_view process_id, std::string_view name, runtime::Buffer<char> && payload)
  {
    std::unique_lock<std::mutex> lock{_conn_mutex};

    auto iter = _connection_data.find(std::string{process_id});
    if(iter == _connection_data.end()) {
      // FIXME: return error?
      _logger->error("Sending message to an unknown process {}!", process_id);
      return;
    }

    Connection* conn = iter->second.get();

    if(!conn->conn) {

      // FIXME: is it only for put message?
      auto put_req = std::make_unique<praas::common::message::PutMessage>();
      put_req->name(name);
      put_req->process_id(_controller.process_id());
      put_req->total_length(payload.len);

      SPDLOG_LOGGER_DEBUG(_logger, "Store pending message of size {}", payload.len);
      conn->pendings_msgs.emplace_back(
        std::move(put_req),
        std::move(payload)
      );

      if(conn->status == Connection::Status::DISCONNECTED) {
        _connect(conn);
      }
    } else {

      praas::common::message::PutMessage put_req;
      put_req.name(name);
      put_req.process_id(_controller.process_id());
      put_req.total_length(payload.len);
      SPDLOG_LOGGER_DEBUG(_logger, "Send PUT message {} with payload len {}", name, payload.len);

      conn->conn->send(put_req.bytes(), put_req.BUF_SIZE);
      conn->conn->send(payload.data(), payload.len);
    }
  }

  void TCPServer::invocation_request(
    std::string_view process_id,
    std::string_view function_name,
    std::string_view invocation_id,
    runtime::Buffer<char> && payload
  )
  {
    std::unique_lock<std::mutex> lock{_conn_mutex};

    auto iter = _connection_data.find(std::string{process_id});
    if(iter == _connection_data.end()) {
      _logger->error("Attempting to send a message to an unknown process {}!", process_id);
      return;
    }

    // FIXME: integrate with put message? similar code
    Connection* conn = iter->second.get();

    if(!conn->conn) {

      // FIXME: is it only for put message?
      auto req = std::make_unique<praas::common::message::InvocationRequest>();
      req->invocation_id(invocation_id);
      req->function_name(function_name);
      req->payload_size(payload.len);
      req->total_length(payload.len);

      SPDLOG_LOGGER_DEBUG(_logger, "Store pending invocation request of {} of size {}", function_name, payload.len);
      conn->pendings_msgs.emplace_back(
        std::move(req),
        std::move(payload)
      );

      if(conn->status == Connection::Status::DISCONNECTED) {
        _connect(conn);
      }

    } else {

      praas::common::message::InvocationRequest req;
      req.invocation_id(invocation_id);
      req.function_name(function_name);
      req.payload_size(payload.len);
      req.total_length(payload.len);
      SPDLOG_LOGGER_DEBUG(_logger, "Send invocation request message with payload len {}", payload.len);

      conn->conn->send(req.bytes(), req.BUF_SIZE);
      conn->conn->send(payload.data(), payload.len);
    }

  }

  void TCPServer::_connect(Connection * conn)
  {

    conn->status = Connection::Status::CONNECTING;
    conn->client = std::make_shared<trantor::TcpClient>(
      this->_server.getLoop(),
      trantor::InetAddress{conn->ip_address, static_cast<uint16_t>(conn->port)},
      "client"
    );

    conn->client->setConnectionCallback(
      [this, connection = conn](const trantor::TcpConnectionPtr &conn) -> void {
          if (conn->connected()) {

            connection->status = Connection::Status::CONNECTED;
            praas::common::message::ProcessConnection req;
            req.process_name(_controller.process_id());
            conn->send(req.bytes(), req.BUF_SIZE);

            // FIXME: make it configurable
            conn->setTcpNoDelay(true);

            connection->conn = conn;

            for(auto & pending_msg : connection->pendings_msgs) {

              auto & msg = std::get<0>(pending_msg);
              auto & buf = std::get<1>(pending_msg);
              conn->send(msg->bytes(), msg->BUF_SIZE);
              if(buf.len > 0) {
                conn->send(buf.data(), buf.len);
              }

            }
            connection->pendings_msgs.clear();

          } else {
            connection->status = Connection::Status::DISCONNECTED;
            SPDLOG_LOGGER_DEBUG(_logger, "Process connection between {} and {} disconnected", conn->localAddr().toIpPort(), conn->peerAddr().toIpPort());
          }
      });

    conn->client->setMessageCallback(
      [this, connection = conn](const trantor::TcpConnectionPtr &conn, trantor::MsgBuffer* buffer) -> void {
        SPDLOG_LOGGER_DEBUG(_logger, "Callback from the client connection! {} bytes to read", buffer->readableBytes());
        _handle_message(conn, buffer);
      }
    );

    SPDLOG_LOGGER_DEBUG(_logger, "Establishing connection to {}:{}", conn->ip_address, conn->port);
    conn->client->connect();
  }

}
