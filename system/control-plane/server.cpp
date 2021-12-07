
#include <chrono>
#include <future>
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
    _http_server(options.https_port, options.ssl_server_cert, options.ssl_server_key, _pool),
    _read_timeout(options.read_timeout),
    _ending(false)
  {
    _listen.open(options.port);
    Workers::init(_redis, _resources, *_backend);
  }

  Server::~Server()
  {
    Workers::free();
    delete _backend;
  }

  void Server::start()
  {
    if (!_listen)
      spdlog::error("Incorrect socket initialization! {}", _listen.last_error_str());

    // Run the HTTP server on another thread
    _http_server.run();

    // FIXME: temporary fix - https://github.com/CrowCpp/Crow/issues/295
    sleep(1);
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = &signal_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);

    while(!_ending) {
      sockpp::tcp_socket conn = _listen.accept();
      conn.read_timeout(std::chrono::microseconds(_read_timeout * 1000));
      if(conn.is_open())
        spdlog::debug("Accepted new connection from {}", conn.peer_address().to_string());

      if(_ending)
        break;
      if (!conn) {
        spdlog::error("Error accepting incoming connection: {}", _listen.last_error_str());
        continue;
      }

      // Ugly fix around the fact that our pool uses std::function
      // std::function cannot accept a non-copyable object :-(
      // And our lambda must be movable only due to dependence on socket.
      _pool.push_task(Worker::worker, new sockpp::tcp_socket{std::move(conn)});
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

