
#ifndef PRAAS_CONTROLL_PLANE_POLLER_HPP
#define PRAAS_CONTROLL_PLANE_POLLER_HPP

#include <praas/common/messages.hpp>
#include <praas/control-plane/handle.hpp>
#include <praas/control-plane/http.hpp>

#include <memory>
#include <sockpp/tcp_socket.h>
#include <string>
#include <unordered_set>

#include <sys/epoll.h>

#include <sockpp/tcp_acceptor.h>
#include <spdlog/spdlog.h>

namespace praas::control_plane {

  class Application;

} // namespace praas::control_plane

namespace praas::control_plane::config {

  class TCPServer;

}

namespace praas::control_plane::tcpserver {

  class TCPServer {
  public:
    TCPServer(config::TCPServer&);

#if defined(WITH_TESTING)
    virtual void add_handle(const process::ProcessHandle*);
#else
    void add_handle(const process::ProcessHandle*);
#endif

    /**
     * @brief Removes handle from the epoll structure. No further messages will be processed.
     *
     * @param {name} [TODO:description]
     */
#if defined(WITH_TESTING)
    virtual void remove_handle(const process::ProcessHandle*);
#else
    void remove_handle(const process::ProcessHandle*);
#endif

    void start();

    void shutdown();

  private:
    void handle_allocation();
    void handle_invocation_result();
    void handle_swap();
    void handle_data_metrics();
    void handle_message(praas::common::Header&);

    std::optional<sockpp::tcp_socket> accept_connection();

    // This assumes that the pointer to the socket does NOT change after
    // submitting to epoll.
    template <typename T>
    bool add_epoll(int handle, T* data, uint32_t epoll_events)
    {
      spdlog::debug(
          "Adding to epoll connection, fd {}, ptr {}, events {}", handle,
          fmt::ptr(static_cast<void*>(data)), epoll_events
      );

      epoll_event event;
      memset(&event, 0, sizeof(epoll_event));
      event.events = epoll_events;
      event.data.ptr = static_cast<void*>(data);

      if (epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, handle, &event) == -1) {
        spdlog::error("Adding socket to epoll failed, reason: {}", strerror(errno));
        return false;
      }
      return true;
    }

    static constexpr int MAX_EPOLL_EVENTS = 32;

    // Accept incoming connections
    sockpp::tcp_acceptor _listen;

    // We use epoll to wait for either new connections from
    // subprocesses/sessions, or to read new messages from subprocesses.
    int _epoll_fd;

    std::atomic<bool> _ending;

    using conn_t = std::tuple<sockpp::tcp_socket *, const process::ProcessHandle *>;
    // There are following requirements on the data structures
    // (1) We need to support adding handles before connections are made, and allow for easy
    // search by process name.
    // (2) We need to store all of the connections to prevent destruction.
    // (3) We need to provide a single pointer to epoll data that contains the connection.
    // (4) Both socket and handle must be available upon receiving data.
    std::unordered_map<std::string, conn_t> _handles;
    std::unordered_map<int, sockpp::tcp_socket> _sockets;
  };

} // namespace praas::control_plane::tcpserver

#endif
