
#include <praas/common/util.hpp>
#include <praas/serving/docker/server.hpp>
#include "praas/common/http.hpp"

namespace praas::serving::docker {

  HttpServer::HttpServer(Options& cfg)
      : _http_port(cfg.http_port), _docker_port(cfg.docker_port), _threads(cfg.threads),
        _max_processes(cfg.max_processes)
  {
    _logger = common::util::create_logger("HttpServer");
  }

  void HttpServer::start()
  {
    _logger->info("Starting HTTP server");
    drogon::app().registerController(shared_from_this());
    drogon::app().setThreadNum(_threads);

    _http_client = common::http::HTTPClientFactory::create_client("http://127.0.0.1", _docker_port);

    _server_thread =
        std::thread{[this]() { drogon::app().addListener("0.0.0.0", _http_port).run(); }};
  }

  void HttpServer::shutdown()
  {
    _logger->info("Stopping HTTP server");
    drogon::app().getLoop()->queueInLoop([]() { drogon::app().quit(); });
    _server_thread.join();
    _logger->info("Stopped HTTP server");
  }

  void HttpServer::wait()
  {
    _server_thread.join();
  }

  void HttpServer::create(
      const drogon::HttpRequestPtr&, std::function<void(const drogon::HttpResponsePtr&)>&& callback
  )
  {
    _http_client.get(
        "/info", {},
        [callback](drogon::ReqResult result, const drogon::HttpResponsePtr& response) {
          Json::Value json;
          std::cerr << result << std::endl;
          std::cerr << response->getBody() << std::endl;
          json["message"] = *response->getJsonObject();
          auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
          resp->setStatusCode(drogon::k200OK);
          callback(resp);
        }
    );
  }

  void HttpServer::swap(
      const drogon::HttpRequestPtr&, std::function<void(const drogon::HttpResponsePtr&)>&& callback
  )
  {
  }

  void HttpServer::cache_image(
      const drogon::HttpRequestPtr&, std::function<void(const drogon::HttpResponsePtr&)>&& callback
  )
  {
  }

} // namespace praas::serving::docker
