
#ifndef __CONTROL_PLANE_HTTP_HPP__
#define __CONTROL_PLANE_HTTP_HPP__

#include <string>
#include <thread>

namespace BS {

  struct thread_pool;

}

namespace praas::http {

  struct HTTPResponse {


  };

  struct HttpServer {
    BS::thread_pool& _pool;
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
