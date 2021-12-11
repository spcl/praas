
#ifndef __CONTROLL_PLANE_SERVER_HPP__
#define __CONTROLL_PLANE_SERVER_HPP__

#include <sockpp/tcp_acceptor.h>
#include <thread_pool.hpp>
#include <redis++.h>
#include <spdlog/spdlog.h>

#include "resources.hpp"
#include "backend.hpp"
#include "http.hpp"

namespace praas::control_plane {

  enum class FunctionBackendType {
    LOCAL = 0,
    AWS
  };

  struct Options {
    std::string redis_addr;
    int threads;
    std::string ip_address;
    int port;
    int read_timeout;
    FunctionBackendType backend;
    std::string local_server;
    bool verbose;
    int https_port;
    std::string ssl_server_cert;
    std::string ssl_server_key;
  };
  Options opts(int, char**);

  struct Server
  {
    static constexpr int MAX_EPOLL_EVENTS = 32;

    sockpp::tcp_acceptor _listen;
    thread_pool _pool;
    sw::redis::Redis _redis;
    Resources _resources;
    backend::Backend* _backend;
    praas::http::HttpServer _http_server;
    int _read_timeout;
    bool _ending;

    // We use epoll to wait for either new connections from subprocesses/sessions,
    // or to read new messages from subprocesses.
    int _epoll_fd;

    // This assumes that the pointer to the socket does NOT change after submitting to epoll.
    template<typename T>
    bool add_epoll(int handle, T* data, uint32_t epoll_events)
    {
      spdlog::debug(
        "Adding to epoll connection, fd {}, ptr {}, events {}",
        handle,
        fmt::ptr(static_cast<void*>(data)),
        epoll_events
      );

      epoll_event event;
      memset(&event, 0, sizeof(epoll_event));
      event.events  = epoll_events;
      event.data.ptr = static_cast<void*>(data);

      if(epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, handle, &event) == -1) {
        spdlog::error("Adding socket to epoll failed, reason: {}", strerror(errno));
        return false;
      }
      return true;
    }

    Server(Options &);
    ~Server();

    void start();
    void shutdown();
  };

}

#endif

