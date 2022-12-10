#ifndef PRAAS_CONTROL_PLANE_HTTP_HPP
#define PRAAS_CONTROL_PLANE_HTTP_HPP

#include <drogon/HttpTypes.h>
#include <memory>
#include <string>
#include <thread>

#include <drogon/drogon.h>
#include <spdlog/spdlog.h>

namespace praas::control_plane {

  namespace worker {
    struct Workers;
  } // namespace worker

  namespace config {
    struct HTTPServer;
  } // namespace config

  struct HttpServer : public
                      drogon::HttpController<HttpServer, false>,
                      std::enable_shared_from_this<HttpServer>
  {
    using request_t = drogon::HttpRequestPtr;
    using callback_t = std::function<void(const drogon::HttpResponsePtr&)>;

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(HttpServer::invoke, "/invoke/{1}/{2}", drogon::Post);
    ADD_METHOD_TO(HttpServer::create_app, "/create_app", drogon::Post);
    METHOD_LIST_END

    HttpServer(
      config::HTTPServer & cfg,
      worker::Workers & workers
    );

    void run();
    void shutdown();

    void create_app(
      const drogon::HttpRequestPtr&,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    void invoke(
      const drogon::HttpRequestPtr&,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
      std::string app_id,
      std::string func_name
    );

    static drogon::HttpResponsePtr failed_response(
      const std::string& reason, drogon::HttpStatusCode code = drogon::k500InternalServerError
    );
    static drogon::HttpResponsePtr correct_response(const std::string& reason);

    int port() const
    {
      return _port;
    }

  private:

    int _port;

    int _threads;

    worker::Workers& _workers;

    std::shared_ptr<spdlog::logger> _logger;
    std::thread _server_thread;
  };
} // namespace praas::control_plane

#endif
