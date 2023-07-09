
#include <praas/common/util.hpp>
#include <praas/serving/docker/server.hpp>

namespace praas::serving::docker {

  HttpServer::HttpServer(Options& cfg)
      : _port(cfg.port), _threads(cfg.threads), _max_processes(cfg.max_processes)
  {
    _logger = common::util::create_logger("HttpServer");
  }

  void HttpServer::start()
  {
    _logger->info("Starting HTTP server");
    drogon::app().registerController(shared_from_this());
    drogon::app().setThreadNum(_threads);
    _server_thread = std::thread{[this]() { drogon::app().addListener("0.0.0.0", _port).run(); }};
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
      const drogon::HttpRequestPtr&, std::function<void(const drogon::HttpResponsePtr&)>&& callback,
      std::string app_id
  )
  {
  }

  void HttpServer::swap(
      const drogon::HttpRequestPtr&, std::function<void(const drogon::HttpResponsePtr&)>&& callback
  )
  {
  }

} // namespace praas::serving::docker
