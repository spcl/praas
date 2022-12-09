#ifndef PRAAS_CONTROL_PLANE_HTTP_HPP
#define PRAAS_CONTROL_PLANE_HTTP_HPP

#include <praas/control-plane/worker.hpp>

#include <memory>
#include <string>
#include <thread>

#include <drogon/drogon.h>
#include <spdlog/spdlog.h>

namespace praas::control_plane {

  struct HTTPResponse {


  };

  struct HttpServer : public
                      drogon::HttpController<HttpServer, false>,
                      std::enable_shared_from_this<HttpServer>
  {
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(HttpServer::invoke, "/invoke/{1}/{2}", drogon::Post);
    METHOD_LIST_END

    HttpServer(
      worker::Workers & workers,
      int port = 443
    );

    void run();
    void shutdown();

    void invoke(
      const drogon::HttpRequestPtr&,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
      std::string app_id,
      std::string func_name
    );

  private:

    int _port;

    worker::Workers& _workers;

    std::shared_ptr<spdlog::logger> _logger;
    std::thread _server_thread;
  };
} // namespace praas::control_plane

#endif
