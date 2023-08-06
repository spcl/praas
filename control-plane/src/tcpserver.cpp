#include <praas/common/util.hpp>
#include <praas/common/uuid.hpp>
#include <praas/control-plane/process.hpp>
#include <praas/control-plane/resources.hpp>
#include <praas/control-plane/tcpserver.hpp>
#include <praas/control-plane/worker.hpp>

#include <praas/common/exceptions.hpp>
#include <praas/common/messages.hpp>
#include <praas/control-plane/config.hpp>

#include <variant>

#include <trantor/net/TcpServer.h>

namespace praas::control_plane::tcpserver {

  TCPServer::TCPServer(const config::TCPServer& options, worker::Workers& workers)
      : _workers(workers),
        _server(_loop_thread.getLoop(), trantor::InetAddress(options.port), "tcpserver"),
        _is_running(true)
  {
    _server.setIoLoopNum(options.io_threads);

    _logger = common::util::create_logger("TCPServer");

    _server.setConnectionCallback([this](const trantor::TcpConnectionPtr& connPtr) {
      if (connPtr->connected()) {
        //_logger->debug("Connected new process from {}", connPtr.get()->peerAddr().toIpPort());
        _logger->info("Connected new process from {}", connPtr.get()->peerAddr().toIpPort());
        _num_connected_processes++;
      } else if (connPtr->disconnected()) {

        // Avoid errors on closure of the server.
        if (!this->_is_running) {
          return;
        }
        _logger->info("Closing a process at {}", connPtr->peerAddr().toIpPort());
        handle_disconnection(connPtr);
      }
    });

    _server.setRecvMessageCallback(
        [this](const trantor::TcpConnectionPtr& connectionPtr, trantor::MsgBuffer* buffer) {
          handle_message(connectionPtr, buffer);
        }
    );

    _logger->info("Starting TCP server at {}", port());

    _loop_thread.run();
    _server.start();
  }

  TCPServer::~TCPServer()
  {
    if (_is_running) {
      shutdown();
    }
  }

  void TCPServer::shutdown()
  {
    _is_running = false;

    _server.stop();
    _loop_thread.getLoop()->quit();
    _loop_thread.wait();
  }

  void TCPServer::add_process(const process::ProcessPtr& ptr)
  {
    _loop_thread.getLoop()->runInLoop([this, ptr]() {
      auto [it, success] = this->_pending_processes.insert(std::make_pair(ptr->name(), ptr));

      if (!success) {
        throw praas::common::ObjectExists{
            fmt::format("Cannot add process {} again to the server!", ptr->name())};
      } else {
        _logger->info("Add pending process {}", ptr->name());
      }
    });
  }

  void TCPServer::remove_process(const process::Process&)
  {
    // Disable the connection
    // Insert the removal
    throw common::NotImplementedError{};
  }

  bool TCPServer::handle_invocation_result(
      ConnectionData& data, trantor::MsgBuffer* buffer,
      const praas::common::message::InvocationResultPtr& req
  )
  {
    // Started, verify there is enough data
    if (data.bytes_to_read == 0) {

      data.bytes_to_read = req.total_length();

      data.cur_msg = req.to_ptr();

      buffer->retrieve(praas::common::message::MessageConfig::BUF_SIZE);
    }

    if (buffer->readableBytes() >= data.bytes_to_read) {

      {
        data.process->write_lock();
        data.process->finish_invocation(
            std::string{req.invocation_id()}, req.return_code(), buffer->peek(), data.bytes_to_read
        );
      }

      buffer->retrieve(data.bytes_to_read);
      data.bytes_to_read = 0;

      return true;
    }
    return false;
  }

  void TCPServer::handle_message(
      const trantor::TcpConnectionPtr& connectionPtr, trantor::MsgBuffer* buffer
  )
  {
    auto conn_data = connectionPtr->getContext<ConnectionData>();
    // FIXME: duplicate code from process - merge?
    if (!conn_data) {

      if (buffer->readableBytes() < praas::common::message::MessageConfig::BUF_SIZE) {
        return;
      }

      praas::common::message::MessagePtr buffer_data{buffer->peek()};
      auto msg = praas::common::message::MessageParser::parse(buffer_data);

      if (std::holds_alternative<common::message::ProcessConnectionPtr>(msg)) {
        handle_connection(connectionPtr, std::get<common::message::ProcessConnectionPtr>(msg));
      } else {
        _logger->error("Ignoring message from an unknown recepient");
      }
      buffer->retrieve(praas::common::message::MessageConfig::BUF_SIZE);

      if (buffer->readableBytes() > 0) {
        handle_message(connectionPtr, buffer);
      }
      return;
    }

    bool consumed = false;

    // Waiting for data

    _logger->info(
        "There are {} bytes to read, {} expected", buffer->readableBytes(), conn_data->bytes_to_read
    );
    if (conn_data->bytes_to_read > 0) {

      auto msg = praas::common::message::MessageParser::parse(conn_data->cur_msg);
      consumed = handle_message(connectionPtr, buffer, *conn_data, msg);
    }
    // Parsing message
    else {

      if (buffer->readableBytes() < praas::common::message::MessageConfig::BUF_SIZE) {
        return;
      }

      praas::common::message::MessagePtr buffer_data{buffer->peek()};
      auto msg = praas::common::message::MessageParser::parse(buffer_data);
      consumed = handle_message(connectionPtr, buffer, *conn_data, msg);
    }

    _logger->info(
        "Consumed message? {} There are {} bytes remaining", consumed, buffer->readableBytes()
    );
    if (consumed && buffer->readableBytes() > 0) {
      handle_message(connectionPtr, buffer);
    }
  }

  bool TCPServer::handle_message(
      const trantor::TcpConnectionPtr& connectionPtr, trantor::MsgBuffer* buffer,
      ConnectionData& data, const praas::common::message::MessageVariants& msg
  )
  {
    return std::visit(
        common::message::overloaded{
            [this, connectionPtr,
             buffer](const common::message::ProcessClosurePtr&) mutable -> bool {
              handle_closure(connectionPtr);
              buffer->retrieve(praas::common::message::MessageConfig::BUF_SIZE);
              return true;
            },
            [this, connectionPtr, buffer,
             data](const common::message::DataPlaneMetricsPtr& metrics) mutable -> bool {
              if (connectionPtr->hasContext()) {
                handle_data_metrics(data.process, metrics);
                buffer->retrieve(praas::common::message::MessageConfig::BUF_SIZE);
              } else {
                spdlog::error(
                    "Ignoring data plane metrics for an unknown process, from {}",
                    connectionPtr->peerAddr().toIpPort()
                );
              }
              return true;
            },
            [this, &data, buffer](const common::message::InvocationResultPtr& req) mutable -> bool {
              return handle_invocation_result(data, buffer, req);
            },
            [this, connectionPtr, buffer,
             data](const common::message::SwapConfirmationPtr&) mutable -> bool {
              if (connectionPtr->hasContext()) {
                handle_swap(data.process);
              } else {
                spdlog::error(
                    "Ignoring swap confirmation metrics for an unknown process, from {}",
                    connectionPtr->peerAddr().toIpPort()
                );
              }
              buffer->retrieve(praas::common::message::MessageConfig::BUF_SIZE);
              return true;
            },
            [this, connectionPtr,
             buffer](common::message::ProcessConnectionPtr& msg) mutable -> bool {
              handle_connection(connectionPtr, msg);
              buffer->retrieve(praas::common::message::MessageConfig::BUF_SIZE);
              return true;
            },
            [this, buffer](const auto&) mutable -> bool {
              _logger->error("Ignore unknown message");
              buffer->retrieve(praas::common::message::MessageConfig::BUF_SIZE);
              return true;
            }},
        msg
    );
  }

  void TCPServer::handle_connection(
      const trantor::TcpConnectionPtr& conn, const common::message::ProcessConnectionPtr& msg
  )
  {
    // unordered_map does not allow to search using string_view for a string key
    // https://stackoverflow.com/questions/34596768/stdunordered-mapfind-using-a-type-different-than-the-key-type
    auto it = _pending_processes.find(std::string{msg.process_name()});
    if (it != _pending_processes.end()) {

      auto process_ptr = (*it).second;
      _pending_processes.erase(it);

      auto process_data = std::make_shared<ConnectionData>(process_ptr);
      _processes.emplace(process_ptr->name(), process_data);

      // Store process reference for future messages
      conn->setContext(process_data);

      _logger->info("Registered process {}", msg.process_name());

      {
        process_ptr->write_lock();
        process_ptr->connect(conn);

        process_ptr->created_callback(std::nullopt);

        // Now send pending invocations. Lock prevents adding more invocations,
        // and by the time we are finishing, the direct connection will be already up
        // and invocation can be submitted directly.
        // Thus, no invocation should be lost.
        process_ptr->send_invocations();
      }

      _num_registered_processes++;

    } else {
      _logger->error("Received registration of an unknown process {}", msg.process_name());
    }
  }

  void TCPServer::handle_closure(const trantor::TcpConnectionPtr& connPtr)
  {
    handle_disconnection(connPtr);
    connPtr->shutdown();
  }

  void TCPServer::handle_disconnection(const trantor::TcpConnectionPtr& connPtr)
  {
    _num_connected_processes--;

    auto process_data = connPtr->getContext<ConnectionData>();
    if (process_data && process_data->process) {
      _logger->debug("Closing process connection for {}", process_data->process->name());
      _num_registered_processes--;
      process_data->process->application().closed_process(process_data->process);
    }
  }

  int TCPServer::port() const
  {
    return _server.address().toPort();
  }

  int TCPServer::num_connected_processes() const
  {
    return _num_connected_processes.load();
  }

  int TCPServer::num_registered_processes() const
  {
    return _num_registered_processes.load();
  }

  void TCPServer::handle_data_metrics(
      const process::ProcessPtr& process_ptr, const praas::common::message::DataPlaneMetricsPtr& msg
  )
  {
    if (process_ptr) {
      process_ptr->update_metrics(
          msg.computation_time(), msg.invocations(), msg.last_invocation_timestamp()
      );
    } else {
      spdlog::error("Ignoring data plane metrics for an unknown process");
    }
  }

  void TCPServer::handle_swap(const process::ProcessPtr& process_ptr)
  {
    if (process_ptr) {
      process_ptr->application().swapped_process(process_ptr->name());
    } else {
      spdlog::error("Ignoring data plane metrics for an unknown process");
    }
  }

} // namespace praas::control_plane::tcpserver
