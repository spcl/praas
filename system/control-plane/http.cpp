
#include <future>

#include <thread_pool.hpp>
#include <spdlog/spdlog.h>

#include "http.hpp"

namespace praas::http {


  HttpServer::HttpServer(int port, std::string server_cert, std::string server_key, thread_pool & pool):
    _pool(pool)
  {
    spdlog::info(
      "Configuring HTTPS sever at port {}, server cert {}, server key {}",
      port, server_cert, server_key
    );
    _server.port(port).ssl_file(server_cert, server_key);
    CROW_ROUTE(_server, "/").methods(crow::HTTPMethod::POST)
    ([]() {
       return "Hello world!";
    });

    // We have our own handling of signals
    _server.signal_clear();
    // FIXME: pass verbose
    //_server.loglevel(crow::LogLevel::ERROR);
  }

  void HttpServer::run()
  {
    _server_thread = std::thread(&crow::App<>::run, &_server);
  }

  void HttpServer::shutdown()
  {
    spdlog::info("Stopping HTTP server");
    _server.stop();
    if(_server_thread.joinable())
      _server_thread.join();
    spdlog::info("Stopped HTTP server");
  }

}
