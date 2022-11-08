#include <praas/control-plane/process.hpp>
#include <praas/control-plane/worker.hpp>
#include <praas/control-plane/tcpserver.hpp>

#include <praas/common/exceptions.hpp>
#include <praas/common/messages.hpp>
#include <praas/control-plane/config.hpp>
#include <sockpp/tcp_socket.h>
#include <trantor/net/TcpServer.h>
#include <variant>

namespace praas::control_plane::tcpserver {

  TCPServer::TCPServer(const config::TCPServer& options, worker::Workers & workers):
    _workers(workers),
    _server(_loop_thread.getLoop(), trantor::InetAddress(options.port), "tcpserver"),
    _is_running(true)
  {
    _server.setIoLoopNum(options.io_threads);


    // FIXME: reference to a thread pool
    //if (enable_listen) {
    //  bool val = _listen.open(options.port);
    //  if(!val) {
    //    spdlog::error("Failed to open a TCP listen at port {}", options.port);
    //    throw common::PraaSException{"Failed TCP listen"};
    //  }
    //}
    //_ending = false;
    _server.setConnectionCallback([this](const trantor::TcpConnectionPtr &connPtr) {
        if (connPtr->connected())
        {
          spdlog::debug("Connected new process from {}", connPtr.get()->peerAddr().toIpPort());
        }
        else if (connPtr->disconnected())
        {
          spdlog::debug("Closing a process at {}", connPtr->peerAddr().toIpPort());
          handle_disconnection(connPtr);
        }
    });

    _loop_thread.run();
    _server.start();
  }

  TCPServer::~TCPServer()
  {
    if(_is_running)
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
    _loop_thread.getLoop()->runInLoop(
      [this, ptr]() {
        this->_pending_processes.insert(std::make_pair(ptr->name(), ptr));
      }
    );
//    auto process = ptr.lock();
//    if (!process) {
//      spdlog::error("Adding non-existing process!");
//      return;
//    }
//
//    // Store weak pointer for future accesses
//    ConcurrentTable<process::ProcessObserver>::rw_acc_t acc;
//    bool inserted = _handles.insert(acc, process->name());
//    if (inserted) {
//      acc->second = std::move(ptr);
//    } else {
//      throw praas::common::ObjectExists(
//          fmt::format("Cannot add process {} again to the server!", process->name())
//      );
//    }
  }

  void TCPServer::remove_process(const process::Process&)
  {
    // Disable the connection
    // Insert the removal
    throw common::NotImplementedError{};
  }

  void TCPServer::handle_disconnection(const trantor::TcpConnectionPtr &connPtr)
  {

  }

  int TCPServer::port() const
  {
    return _server.address().toPort();
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
//      process::ProcessObserver* process, praas::common::message::Message&& msg, sockpp::tcp_socket && socket
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
//          spdlog::error("Received acceptation of an unknown process {}", conn_msg.process_name());
//        } else {
//          auto process_ptr = acc->second.lock();
//          if(process_ptr != nullptr) {
//            auto& conn = process_ptr->connection();
//            int fd = socket.handle();
//            {
//              conn.write_lock();
//              conn.connection = std::move(sockpp::tcp_socket{});
//            }
//            epoll_mod(_epoll_fd, fd, &acc->second, EPOLLIN | EPOLLPRI | EPOLLRDHUP | EPOLLONESHOT);
//          } else {
//            spdlog::error("Received confirmation message from process {}, but it is deleted", conn_msg.process_name());
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
//      int events_count = epoll_wait(_epoll_fd, events.data(), MAX_EPOLL_EVENTS, EPOLL_TIMEOUT);
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
