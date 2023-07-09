
#ifndef PRAAS_SERVING_DOCKER_SERVER_HPP
#define PRAAS_SERVING_DOCKER_SERVER_HPP

#include <thread>

#include <drogon/drogon.h>
#include <spdlog/spdlog.h>

namespace praas::serving::docker {

  struct Options {
    int max_processes;
    int port;
    int threads;
    bool verbose;
  };
  Options opts(int, char**);

  struct HttpServer : public drogon::HttpController<HttpServer, false>,
                      std::enable_shared_from_this<HttpServer> {
    using request_t = drogon::HttpRequestPtr;
    using callback_t = std::function<void(const drogon::HttpResponsePtr&)>;

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(HttpServer::create, "/create/{1}/", drogon::Post);
    ADD_METHOD_TO(HttpServer::swap, "/swap", drogon::Post);
    METHOD_LIST_END

    HttpServer(Options&);

    void start();
    void shutdown();
    void wait();

    void create(
        const drogon::HttpRequestPtr&,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback, std::string app_id
    );

    void swap(
        const drogon::HttpRequestPtr&,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    int port() const
    {
      return _port;
    }

  private:
    int _port;
    int _threads;
    int _max_processes;

    std::thread _server_thread;
    std::shared_ptr<spdlog::logger> _logger;
  };

} // namespace praas::serving::docker

#endif
