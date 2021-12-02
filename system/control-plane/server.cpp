
#include <spdlog/spdlog.h>

#include "server.hpp"

namespace praas::control_plane {

  Server::Server(Options & options):
    _listen(options.port),
    _ending(false)
  {
  }

  void Server::start()
  {
    if (!_listen)
      spdlog::error("Incorrect socket initialization! {}", _listen.last_error_str());

    while(!_ending) {
      sockpp::tcp_socket conn = _listen.accept();
      if (!conn)
        spdlog::error("Error accepting incoming connection: {}", _listen.last_error_str());

      char buf[16];
      ssize_t n = conn.read(buf, sizeof(buf));

      conn.write("hello client", 13);
    }
  }

  void Server::shutdown()
  {
    _ending = true;
    _listen.shutdown();
  }

}

