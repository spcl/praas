
#ifndef __CONTROL_PLANE_HTTP_HPP__
#define __CONTROL_PLANE_HTTP_HPP__

#include <string>
#include <thread>

#include <crow.h>

namespace BS {

  struct thread_pool;

}

namespace praas::http {

  struct HttpServer {
    BS::thread_pool& _pool;
    crow::SimpleApp _server;
    std::thread _server_thread;

    HttpServer(
        int port, std::string server_cert, std::string server_key,
        BS::thread_pool&, bool verbose
    );

    void run();
    void shutdown();
  };
} // namespace praas::http

#endif
