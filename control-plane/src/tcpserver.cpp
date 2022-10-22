#include <praas/control-plane/tcpserver.hpp>

#include <praas/common/exceptions.hpp>
#include <praas/control-plane/config.hpp>

namespace praas::control_plane::tcpserver {

  TCPServer::TCPServer(config::TCPServer& options)
  {
    _listen.open(options.port);
  }

  void TCPServer::add_handle(const process::ProcessHandle*)
  {
    throw common::NotImplementedError{};
  }

  void TCPServer::remove_handle(const process::ProcessHandle*)
  {
    throw common::NotImplementedError{};
  }

  std::optional<sockpp::tcp_socket> TCPServer::accept_connection()
  {
    sockpp::tcp_socket conn = _listen.accept();

    if (conn.is_open()) {
      spdlog::debug("Accepted new connection from {}.", conn.peer_address().to_string());
    }

    if (!conn) {
      spdlog::error("Error accepting incoming connection: {}", _listen.last_error_str());
    } else {
      add_epoll<void>(conn.handle(), nullptr, EPOLLIN | EPOLLPRI);
      _sockets.emplace(conn.handle(), std::move(conn));
    }

    return conn;
  }

  void TCPServer::start()
  {

    if (!_listen) {
      spdlog::error("Incorrect socket initialization! {}", _listen.last_error_str());
      return;
    }

    // size is ignored by Linux
    _epoll_fd = epoll_create(255);
    if (_epoll_fd < 0) {
      spdlog::error("Incorrect epoll initialization! {}", strerror(errno));
      return;
    }
    if (!add_epoll(_listen.handle(), &_listen, EPOLLIN | EPOLLPRI)) {
      return;
    }

    epoll_event events[MAX_EPOLL_EVENTS];
    while (!_ending) {

      int events_count = epoll_wait(_epoll_fd, events, MAX_EPOLL_EVENTS, 0);

      // Finish if we failed (but we were not interrupted), or when end was
      // requested.
      if (_ending || (events_count == -1 && errno != EINVAL)) {
        break;
      }

      for (int i = 0; i < events_count; ++i) {

        if (events[i].data.ptr == &_listen) {

          accept_connection();

        } else {

          // FIXME: aggregate multiple messages from the same connection
          praas::common::Header msg;
          auto [socket, handle] = *static_cast<conn_t*>(events[i].data.ptr);
          ssize_t recv_data = socket->read_n(msg.data.data(), praas::common::Header::BUF_SIZE);

        }
      }
    }
  }

  void TCPServer::shutdown()
  {
    _ending = true;
    spdlog::info("Closing TCP poller.");
    _listen.shutdown();
    spdlog::info("Closed TCP poller.");
  }

} // namespace praas::control_plane::tcpserver
