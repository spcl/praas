
#include <chrono>
#include <future>
#include <sys/epoll.h>

#include <spdlog/spdlog.h>

#include "server.hpp"
#include "worker.hpp"
#include "http.hpp"

extern void signal_handler(int);

namespace praas::control_plane {

  Server::Server(Options & options):
    _pool(options.threads),
    _redis(options.redis_addr),
    _backend(backend::Backend::construct(options)),
    _http_server(options.https_port, options.ssl_server_cert, options.ssl_server_key, _pool, options.verbose),
    _read_timeout(options.read_timeout),
    _ending(false)
  {
    _listen.open(options.port);
    Workers::init(*this, _redis, _resources, *_backend);
  }

  Server::~Server()
  {
    Workers::free();
    delete _backend;
  }

  void Server::start()
  {
    if (!_listen) {
      spdlog::error("Incorrect socket initialization! {}", _listen.last_error_str());
      return;
    }

    _epoll_fd = epoll_create(255);
    if(_epoll_fd < 0) {
      spdlog::error("Incorrect epoll initialization! {}", strerror(errno));
      return;
    }
    if(!add_epoll(_listen.handle(), &_listen, EPOLLIN | EPOLLPRI))
      return;

    // Run the HTTP server on another thread
    _http_server.run();

    // FIXME: temporary fix - https://github.com/CrowCpp/Crow/issues/295
    sleep(1);
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = &signal_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);

    epoll_event events[MAX_EPOLL_EVENTS];
    while(!_ending) {

      int events_count = epoll_wait(_epoll_fd, events, MAX_EPOLL_EVENTS, 0);

      // Finish if we failed (but we were not interrupted), or when end was requested.
      if(_ending || (events_count == -1 && errno != EINVAL))
        break;

      for(int i = 0; i < events_count; ++i) {

        // Accept connection
        if(events[i].data.ptr == static_cast<void*>(&_listen)) {

          sockpp::tcp_socket conn = _listen.accept();
          // FIXME: disable until we figure out what to do with epoll
          //conn.read_timeout(std::chrono::microseconds(_read_timeout * 1000));
          if(conn.is_open())
            spdlog::debug("Accepted new connection from {}.", conn.peer_address().to_string());

          if (!conn) {
            spdlog::error("Error accepting incoming connection: {}", _listen.last_error_str());
            continue;
          }

          // Ugly fix around the fact that our pool uses std::function
          // std::function cannot accept a non-copyable object :-(
          // And our lambda must be movable only due to dependence on socket.
          _pool.push_task(Worker::worker, new sockpp::tcp_socket{std::move(conn)});
        }
        // New message from a process. 
        else {

          // FIXME: aggregate multiple messages from the same connection
          praas::common::Header msg; 
          Process* process = static_cast<Process*>(events[i].data.ptr);
          sockpp::tcp_socket* conn = &process->connection;

          //if(process->busy.load()) {
          //  spdlog::debug("Ignore activity on busy {}, events {}", conn->peer_address().to_string(), events[i].events);
          //  continue;
          //}
          //spdlog::debug("New activity on {}, events {}", conn->peer_address().to_string(), events[i].events);

          ssize_t recv_data = conn->read_n(msg.data, praas::common::Header::BUF_SIZE);

          // Store process sending the message in user-provided data
          _pool.push_task(Worker::handle_message, process, msg, recv_data);

        }
      }
    }
  }

  void Server::shutdown()
  {
    _ending = true;
    spdlog::info("Closing control plane.");
    _listen.shutdown();
    _http_server.shutdown();
    spdlog::info("Closed control plane.");
  }

}

