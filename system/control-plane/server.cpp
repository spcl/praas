
#include <spdlog/spdlog.h>

#include "server.hpp"
#include "worker.hpp"

namespace praas::control_plane {

  Server::Server(Options & options):
    _pool(options.threads),
    _redis(options.redis_addr),
    _backend(backend::Backend::construct(options)),
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

    while(!_ending) {
      sockpp::tcp_socket conn = _listen.accept();

      if(_ending)
        break;
      if (!conn)
        spdlog::error("Error accepting incoming connection: {}", _listen.last_error_str());

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
  }

}

