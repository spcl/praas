#include <praas/control-plane/process.hpp>
#include <praas/control-plane/resources.hpp>
#include <praas/control-plane/tcpserver.hpp>
#include <praas/control-plane/worker.hpp>

#include <praas/common/exceptions.hpp>
#include <praas/common/messages.hpp>
#include <praas/control-plane/config.hpp>

#include <variant>

#include <sockpp/tcp_socket.h>
#include <trantor/net/TcpServer.h>

namespace praas::control_plane::tcpserver {

  TCPServer::TCPServer(const config::TCPServer& options, worker::Workers& workers)
      : _workers(workers),
        _server(_loop_thread.getLoop(), trantor::InetAddress(options.port), "tcpserver"),
        _is_running(true)
  {
    _server.setIoLoopNum(options.io_threads);

    // FIXME: reference to a thread pool
    // if (enable_listen) {
    //  bool val = _listen.open(options.port);
    //  if(!val) {
    //    spdlog::error("Failed to open a TCP listen at port {}", options.port);
    //    throw common::PraaSException{"Failed TCP listen"};
    //  }
    //}
    //_ending = false;
    _server.setConnectionCallback([this](const trantor::TcpConnectionPtr& connPtr) {
      if (connPtr->connected()) {
        spdlog::debug("Connected new process from {}", connPtr.get()->peerAddr().toIpPort());
        _num_connected_processes++;
      } else if (connPtr->disconnected()) {
        spdlog::debug("Closing a process at {}", connPtr->peerAddr().toIpPort());
        handle_disconnection(connPtr);
      }
    });

    _server.setRecvMessageCallback(
        [this](const trantor::TcpConnectionPtr& connectionPtr, trantor::MsgBuffer* buffer) {
          handle_message(connectionPtr, buffer);
        }
    );

    _loop_thread.run();
    _server.start();
  }

  TCPServer::~TCPServer()
  {
    if (_is_running)
      shutdown();
  }

  void TCPServer::shutdown()
  {
    _server.stop();
    _loop_thread.getLoop()->quit();
    _loop_thread.wait();

    _is_running = false;
  }

  void TCPServer::add_process(const process::ProcessPtr& ptr)
  {
    _loop_thread.getLoop()->runInLoop([this, ptr]() {
      auto [it, success] = this->_pending_processes.insert(std::make_pair(ptr->name(), ptr));

      if (!success) {
        throw praas::common::ObjectExists{
            fmt::format("Cannot add process {} again to the server!", ptr->name())};
      }
    });
  }

  void TCPServer::remove_process(const process::Process&)
  {
    // Disable the connection
    // Insert the removal
    throw common::NotImplementedError{};
  }

  void TCPServer::handle_message(
      const trantor::TcpConnectionPtr& connectionPtr, trantor::MsgBuffer* buffer
  )
  {
    while (buffer->readableBytes() >= praas::common::message::Message::BUF_SIZE) {

      auto msg = praas::common::message::Message::parse_message(buffer->peek());

      std::visit(
          common::message::overloaded{
              [this, connectionPtr](const common::message::ProcessClosureParsed&) mutable -> void {
                handle_closure(connectionPtr);
              },
              [this, connectionPtr](const common::message::DataPlaneMetricsParsed& metrics
              ) mutable -> void {
                if (connectionPtr->hasContext()) {
                  handle_data_metrics(connectionPtr->getContext<process::Process>(), metrics);
                } else {
                  spdlog::error(
                      "Ignoring data plane metrics for an unknown process, from {}",
                      connectionPtr->peerAddr().toIpPort()
                  );
                }
              },
              [](const common::message::InvocationResultParsed&) mutable -> void {
                // FIXME:
              },
              [](const common::message::SwapRequestParsed&) mutable -> void {
                spdlog::error("Ignoring swap request message - bug?");
              },
              [this, connectionPtr](const common::message::SwapConfirmationParsed&) mutable -> void {
                if (connectionPtr->hasContext()) {
                  handle_swap(connectionPtr->getContext<process::Process>());
                } else {
                  spdlog::error(
                      "Ignoring swap confirmation metrics for an unknown process, from {}",
                      connectionPtr->peerAddr().toIpPort()
                  );
                }
              },
              [this, connectionPtr](const common::message::ProcessConnectionParsed& msg
              ) mutable -> void { handle_connection(connectionPtr, msg); },
              [](const common::message::InvocationRequestParsed&) mutable -> void {},
              [](const common::message::PutMessageParsed&) mutable -> void {},
              [](const common::message::ApplicationUpdateParsed&) mutable -> void {}},
          msg
      );

      buffer->retrieve(praas::common::message::Message::BUF_SIZE);
    }
  }

  void TCPServer::handle_connection(
      const trantor::TcpConnectionPtr& conn, const common::message::ProcessConnectionParsed& msg
  )
  {
    // unordered_map does not allow to search using string_view for a string key
    // https://stackoverflow.com/questions/34596768/stdunordered-mapfind-using-a-type-different-than-the-key-type
    auto it = _pending_processes.find(std::string{msg.process_name()});
    if (it != _pending_processes.end()) {

      auto process_ptr = (*it).second;
      _pending_processes.erase(it);
      {
        process_ptr->read_lock();
        process_ptr->connect(conn);
      }
      // Store process reference for future messages
      conn->setContext(std::move(process_ptr));

      _num_registered_processes++;

      spdlog::debug("Registered process {}", msg.process_name());

    } else {
      spdlog::error("Received registration of an unknown process {}", msg.process_name());
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

    auto process_ptr = connPtr->getContext<process::Process>();
    if (process_ptr) {
      spdlog::debug("Closing process connection for {}", process_ptr->name());
      _num_registered_processes--;
      process_ptr->application().closed_process(process_ptr);
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
      const process::ProcessPtr& process_ptr,
      const praas::common::message::DataPlaneMetricsParsed& msg
  )
  {
    if (process_ptr) {
      process_ptr->update_metrics(msg.computation_time(), msg.invocations(), msg.last_invocation_timestamp());
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

  //  std::optional<sockpp::tcp_socket> TCPServer::accept_connection()
  //  {
  //    sockpp::tcp_socket conn = _listen.accept();
  //
  //    if (conn.is_open()) {
  //      spdlog::debug("Accepted new connection from {}.", conn.peer_address().to_string());
  //    }
  //
  //    if (!conn) {
  //      spdlog::error("Error accepting incoming connection: {}", _listen.last_error_str());
  //    } else {
  //      // We are not going to store the socket wrapper for now.
  //      epoll_add<void>(_epoll_fd, conn.release(), nullptr, EPOLLIN | EPOLLPRI);
  //    }
  //
  //    return conn;
  //  }
  //
  //  void TCPServer::handle_message(
  //      process::ProcessObserver* process, praas::common::message::Message&& msg,
  //      sockpp::tcp_socket && socket
  //  )
  //  {
  //    // Is there a process attached? Then let the process handle the message.
  //    if (process != nullptr) {
  //      socket.release();
  //      _workers.add_task(process, std::move(msg));
  //    }
  //    // No pointer? Then we only accept "connect" message"
  //    else {
  //
  //      auto parsed_msg = msg.parse();
  //      // Connects the newly allocated socket to a process handle.
  //      // Requires a write access to the process data plane connection structure.
  //      if (std::holds_alternative<common::message::ProcessConnectionParsed>(parsed_msg)) {
  //
  //        auto& conn_msg = std::get<common::message::ProcessConnectionParsed>(parsed_msg);
  //
  //        // We do not really modify it here, but we want to get a non-const pointer to
  //        // store in epoll.
  //        ConcurrentTable<process::ProcessObserver>::rw_acc_t acc;
  //        _handles.find(acc, std::string{conn_msg.process_name()});
  //        if(acc.empty()) {
  //          spdlog::error("Received acceptation of an unknown process {}",
  //          conn_msg.process_name());
  //        } else {
  //          auto process_ptr = acc->second.lock();
  //          if(process_ptr != nullptr) {
  //            auto& conn = process_ptr->connection();
  //            int fd = socket.handle();
  //            {
  //              conn.write_lock();
  //              conn.connection = std::move(sockpp::tcp_socket{});
  //            }
  //            epoll_mod(_epoll_fd, fd, &acc->second, EPOLLIN | EPOLLPRI | EPOLLRDHUP |
  //            EPOLLONESHOT);
  //          } else {
  //            spdlog::error("Received confirmation message from process {}, but it is deleted",
  //            conn_msg.process_name());
  //            // FIXME: delete from epoll
  //          }
  //        }
  //
  //      } else {
  //        spdlog::error("Received unacceptable message from an unregistered process");
  //      }
  //
  //    }
  //  }
  //
  //  void TCPServer::handle_error(const process::ProcessObserver* process, sockpp::tcp_socket &&)
  //  {
  //    // FIXME:
  //    spdlog::error("Received incomplete header data");
  //    throw common::NotImplementedError{};
  //  }
  //
  //  void TCPServer::start()
  //  {
  //
  //    if (!_listen) {
  //      spdlog::error("Incorrect socket initialization! {}", _listen.last_error_str());
  //      return;
  //    }
  //
  //    // size is ignored by Linux
  //    _epoll_fd = epoll_create(255);
  //    if (_epoll_fd < 0) {
  //      spdlog::error("Incorrect epoll initialization! {}", strerror(errno));
  //      return;
  //    }
  //    if (!epoll_add(_epoll_fd, _listen.handle(), &_listen, EPOLLIN | EPOLLPRI)) {
  //      spdlog::error("ok");
  //      return;
  //    }
  //
  //    std::array<epoll_event, MAX_EPOLL_EVENTS> events;
  //    while (true) {
  //
  //      int events_count = epoll_wait(_epoll_fd, events.data(), MAX_EPOLL_EVENTS,
  //      EPOLL_TIMEOUT);
  //
  //      // Finish if we failed (but we were not interrupted), or when end was
  //      // requested.
  //      if (_ending || (events_count == -1 && errno != EINVAL)) {
  //        break;
  //      }
  //
  //      for (int i = 0; i < events_count; ++i) {
  //
  //        // FIXME: Handle event errors here
  //
  //        if (events[i].data.ptr == &_listen) {
  //
  //          accept_connection();
  //
  //        } else {
  //
  //          praas::common::message::Message msg;
  //          sockpp::tcp_socket socket{events[i].data.fd};
  //          auto* process = static_cast<process::ProcessObserver*>(events[i].data.ptr);
  //
  //          ssize_t recv_data = socket.read_n(msg.data.data(), decltype(msg)::BUF_SIZE);
  //          if (recv_data == decltype(msg)::BUF_SIZE) {
  //            handle_message(process, std::move(msg), std::move(socket));
  //          } else {
  //            handle_error(process, std::move(socket));
  //          }
  //        }
  //      }
  //    }
  //  }
  //
  //  void TCPServer::shutdown()
  //  {
  //    _ending = true;
  //    spdlog::info("Closing TCP poller.");
  //    _listen.shutdown();
  //    spdlog::info("Closed TCP poller.");
  //  }

} // namespace praas::control_plane::tcpserver
