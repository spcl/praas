
#ifndef PRAAS_SERVING_DOCKER_SERVER_HPP
#define PRAAS_SERVING_DOCKER_SERVER_HPP

#include <praas/common/http.hpp>
#include <praas/serving/docker/containers.hpp>

#include <optional>
#include <thread>

#include <drogon/drogon.h>
#include <spdlog/spdlog.h>

namespace cereal {

  struct JSONInputArchive;

} // namespace cereal

namespace praas::serving::docker {

  struct Options {
    int max_processes;
    int http_port;
    std::string server_ip;
    int docker_port;
    int process_port;
    int threads;
    bool verbose;

    void serialize(cereal::JSONInputArchive&);
  };
  std::optional<Options> opts(int, char**);

  struct HttpServer : public drogon::HttpController<HttpServer, false>,
                      std::enable_shared_from_this<HttpServer> {
    using request_t = drogon::HttpRequestPtr;
    using callback_t = std::function<void(const drogon::HttpResponsePtr&)>;

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(HttpServer::create, "/create?process={1}", drogon::Post);
    ADD_METHOD_TO(HttpServer::kill, "/kill?process={1}", drogon::Post);
    ADD_METHOD_TO(HttpServer::cache_image, "/cache?image={1}", drogon::Post);
    ADD_METHOD_TO(HttpServer::list_containers, "/list", drogon::Get);
    METHOD_LIST_END

    HttpServer(Options&);

    void start();
    void shutdown();
    void wait();

    void create(
        const drogon::HttpRequestPtr&,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback, const std::string& process
    );

    void kill(
        const drogon::HttpRequestPtr&,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback, const std::string& process
    );

    void cache_image(
        const drogon::HttpRequestPtr&,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback, std::string image
    );

    void list_containers(
        const drogon::HttpRequestPtr&,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

  private:
    void _start_container(
        const std::string& proc_name, const std::string& container_id,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );
    void _inspect_container(
        const std::string& proc_name, const std::string& container_id,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    void _configure_ports(Json::Value& body);

    void _kill_all();

    Options opts;

    praas::common::http::HTTPClient _http_client;

    std::thread _server_thread;
    std::shared_ptr<spdlog::logger> _logger;

    Processes _processes;
  };

} // namespace praas::serving::docker

#endif
