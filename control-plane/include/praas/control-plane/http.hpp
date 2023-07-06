#ifndef PRAAS_CONTROL_PLANE_HTTP_HPP
#define PRAAS_CONTROL_PLANE_HTTP_HPP

#include <memory>
#include <string>
#include <thread>

#include <drogon/HttpTypes.h>
#include <drogon/drogon.h>
#include <spdlog/spdlog.h>

namespace praas::control_plane {

  namespace worker {
    struct Workers;
  } // namespace worker

  namespace config {
    struct HTTPServer;
  } // namespace config

  struct HttpServer : public drogon::HttpController<HttpServer, false>,
                      std::enable_shared_from_this<HttpServer> {
    using request_t = drogon::HttpRequestPtr;
    using callback_t = std::function<void(const drogon::HttpResponsePtr&)>;

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(HttpServer::create_app, "/apps/{1}", drogon::Put);
    ADD_METHOD_TO(HttpServer::create_process, "/apps/{1}/processes/{2}", drogon::Put);
    ADD_METHOD_TO(HttpServer::delete_process, "/apps/{1}/processes/{2}/delete", drogon::Post);
    ADD_METHOD_TO(HttpServer::swap_process, "/apps/{1}/processes/{2}/swap", drogon::Post);
    ADD_METHOD_TO(HttpServer::invoke, "/apps/{1}/processes/{2}/invoke", drogon::Post);
    ADD_METHOD_TO(HttpServer::list_processes, "/apps/{1}/processes", drogon::Get);
    ADD_METHOD_TO(HttpServer::list_apps, "/apps", drogon::Get);
    METHOD_LIST_END

    HttpServer(config::HTTPServer& cfg, worker::Workers& workers);

    void run();
    void shutdown();

    void create_app(
        const drogon::HttpRequestPtr& request,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback, const std::string& app_name
    );

    void create_process(
        const drogon::HttpRequestPtr& request,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback, const std::string& app_name,
        const std::string& process_name
    );

    void delete_process(
        const drogon::HttpRequestPtr& request,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback, const std::string& app_name,
        const std::string& process_name
    );

    void swap_process(
        const drogon::HttpRequestPtr&,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback, const std::string& app_id,
        const std::string& process_id
    );

    void invoke(
        const drogon::HttpRequestPtr&,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback, std::string app_id,
        std::string func_name
    );

    void list_processes(
        const drogon::HttpRequestPtr&,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback, std::string app_id
    );

    void list_apps(
        const drogon::HttpRequestPtr&,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
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
