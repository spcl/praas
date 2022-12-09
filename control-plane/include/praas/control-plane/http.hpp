
#ifndef PRAAS_CONTROL_PLANE_HTTP_HPP
#define PRAAS_CONTROL_PLANE_HTTP_HPP

#include <string>
#include <thread>

#include <drogon/drogon.h>
#include <spdlog/spdlog.h>

namespace praas::control_plane {

  struct HTTPResponse {


  };

  struct HttpServer : drogon::HttpController<HttpServer>
  {
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(HttpServer::invoke, "/invoke", drogon::Post);
    METHOD_LIST_END

    HttpServer(
        int port = 443
        //std::string server_cert, std::string server_key,
    );

    void run();
    void shutdown();

    void invoke(const drogon::HttpRequestPtr&, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
  private:

    int _port;

    std::shared_ptr<spdlog::logger> _logger;
    std::thread _server_thread;
  };
} // namespace praas::control_plane

#endif
