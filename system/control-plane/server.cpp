
#include <spdlog/spdlog.h>

#include "server.hpp"
#include "worker.hpp"

namespace praas::control_plane {

  Server::Server(Options & options):
    _listen(options.port),
    _pool(options.threads),
    _redis(new sw::redis::Redis{options.redis_addr}),
    _ending(false)
  {
  }

  Server::~Server()
  {
    delete _redis;
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
      _pool.push_task(worker, new sockpp::tcp_socket{std::move(conn)}, _redis);
    }
  }

  void Server::shutdown()
  {
    _ending = true;
    spdlog::info("Closing control plane.");
    _listen.shutdown();
  }

}

