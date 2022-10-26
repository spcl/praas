
#ifndef PRAAS_CONTROLL_PLANE_POLLER_HPP
#define PRAAS_CONTROLL_PLANE_POLLER_HPP

#include <praas/common/messages.hpp>
#include <praas/control-plane/process.hpp>

#include <memory>
#include <sockpp/tcp_socket.h>
#include <string>
#include <unordered_set>

#include <sys/epoll.h>

#include <sockpp/tcp_acceptor.h>
#include <spdlog/spdlog.h>
#include <tbb/concurrent_hash_map.h>

namespace praas::control_plane {

  class Application;

} // namespace praas::control_plane

namespace praas::control_plane::config {

  class TCPServer;

}

namespace praas::control_plane::worker {

  class Workers;

}

namespace praas::control_plane::tcpserver {

  namespace {

    // This assumes that the pointer to the socket does NOT change after
    // submitting to epoll.
    template <typename T>
    bool epoll_apply(int epoll_fd, int fd, T* data, uint32_t epoll_events, int flags)
    {
      spdlog::debug(
          "Adding to epoll connection, fd {}, ptr {}, events {}", fd,
          // NOLINTNEXTLINE
          fmt::ptr(reinterpret_cast<void*>(data)), epoll_events
      );

      epoll_event event{};
      memset(&event, 0, sizeof(epoll_event));
      event.events = epoll_events;
      // NOLINTNEXTLINE
      event.data.ptr = reinterpret_cast<void*>(data);

      if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
        spdlog::error("Adding socket to epoll failed, reason: {}", strerror(errno));
        return false;
      }
      return true;
    }

    template <typename T>
    bool epoll_add(int epoll_fd, int fd, T* data, uint32_t epoll_events)
    {
      epoll_apply(epoll_fd, fd, data, epoll_events, EPOLL_CTL_ADD);
    }

    template <typename T>
    bool epoll_mod(int epoll_fd, int fd, T* data, uint32_t epoll_events)
    {
      epoll_apply(epoll_fd, fd, data, epoll_events, EPOLL_CTL_MOD);
    }
  }

  template <typename Value, typename Key = std::string>
  struct ConcurrentTable {

    // IntelTBB concurrent hash map, with a default string key
    using table_t = oneapi::tbb::concurrent_hash_map<Key, Value>;

    // Equivalent to receiving a read-write lock. Should be used only for
    // modifying contents.
    using rw_acc_t = typename oneapi::tbb::concurrent_hash_map<Key, Value>::accessor;

    // Read lock. Guarantees that data is safe to access, as long as we keep the
    // accessor
    using ro_acc_t = typename oneapi::tbb::concurrent_hash_map<Key, Value>::const_accessor;
  };

  class TCPServer {
  public:
    TCPServer(const config::TCPServer&, worker::Workers & workers, bool enable_listen = true);

#if defined(WITH_TESTING)
    virtual void add_process(process::ProcessObserver && ptr);
#else
    void add_process(process::ProcessObserver && ptr);
#endif

    /**
     * @brief Removes handle from the epoll structure. No further messages will be processed.
     *
     * @param {name} [TODO:description]
     */
#if defined(WITH_TESTING)
    virtual void remove_process(const process::Process &);
#else
    void remove_process(const process::Process &);
#endif

    void start();

    void shutdown();

  protected:

    void handle_message(process::ProcessObserver* process, praas::common::message::Message&& msg, sockpp::tcp_socket &&);

    void handle_error(const process::ProcessObserver* process, sockpp::tcp_socket &&);

    std::optional<sockpp::tcp_socket> accept_connection();

    static constexpr int MAX_EPOLL_EVENTS = 32;
    static constexpr int EPOLL_TIMEOUT = 1000;

    // Accept incoming connections
    sockpp::tcp_acceptor _listen;

    // Thread pool
    worker::Workers& _workers;

    // We use epoll to wait for either new connections from
    // subprocesses/sessions, or to read new messages from subprocesses.
    int _epoll_fd;

    std::atomic<bool> _ending;

    // There are following requirements on the data structures
    // (1) We need to support adding handles before connections are made to link new connection
    // with a socket.
    // (2) When a new connection is established, we need to store it.
    // (3) When the process announces its name, we need to link to an existing handle via a name.
    // (4) We need to store all of the connections to prevent destruction.
    // (5) We need to provide a single pointer to epoll data that contains the connection.
    // (6) Both socket and handle must be available upon receiving data.
    //
    // A single hash table would be sufficient if it wasn't for the fact that we need to
    // store connections *before* process is recognized.

    ConcurrentTable<process::ProcessObserver>::table_t _handles;
  };

} // namespace praas::control_plane::tcpserver

#endif
