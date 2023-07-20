
#ifndef PRAAS_SERVING_DOCKER_SERVER_HPP
#define PRAAS_SERVING_DOCKER_SERVER_HPP

#include <praas/common/http.hpp>
#include <praas/serving/docker/containers.hpp>

#include <thread>

#include <drogon/drogon.h>
#include <spdlog/spdlog.h>

namespace praas::serving::docker {

  struct Options {
    int max_processes;
    int http_port;
    int docker_port;
    int threads;
    bool verbose;
  };
  Options opts(int, char**);

  struct HttpServer : public drogon::HttpController<HttpServer, false>,
                      std::enable_shared_from_this<HttpServer> {
    using request_t = drogon::HttpRequestPtr;
    using callback_t = std::function<void(const drogon::HttpResponsePtr&)>;

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(HttpServer::create, "/create", drogon::Post);
    ADD_METHOD_TO(HttpServer::kill, "/kill", drogon::Post);
    ADD_METHOD_TO(HttpServer::cache_image, "/cache?image={1}", drogon::Post);
    METHOD_LIST_END

    HttpServer(Options&);

    void start();
    void shutdown();
    void wait();

    void create(
        const drogon::HttpRequestPtr&,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    void kill(
        const drogon::HttpRequestPtr&,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    void cache_image(
        const drogon::HttpRequestPtr&,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback, std::string image
    );

  private:
    void _start_container(
        const std::string& proc_name, const std::string& container_id,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    int _http_port;
    int _docker_port;
    int _threads;
    int _max_processes;

    praas::common::http::HTTPClient _http_client;

    std::thread _server_thread;
    std::shared_ptr<spdlog::logger> _logger;

    Processes _processes;
  };

} // namespace praas::serving::docker

#endif
