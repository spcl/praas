#include <praas/control-plane/server.hpp>

#include <praas/control-plane/http.hpp>
#include <praas/control-plane/tcpserver.hpp>
#include <praas/control-plane/worker.hpp>

#include <chrono>
#include <future>

#include <sys/epoll.h>

#include <spdlog/spdlog.h>

extern void signal_handler(int);

namespace praas::control_plane {

  std::shared_ptr<Server> Server::_instance = nullptr;

  Server::Server(config::Config& cfg):
    _deployment(deployment::Deployment::construct(cfg)),
    _backend(backend::Backend::construct(cfg, *_deployment)),
    _workers(cfg.workers, *_backend, *_deployment, _resources),
    _tcp_server(cfg.tcpserver, _workers),
    _downscaler(_workers, _deployment.get(), cfg.down_scaler),
    _http_server(std::make_shared<HttpServer>(cfg.http, _workers))
  {
    _logger = common::util::create_logger("Server");


    _workers.attach_tcpserver(_tcp_server);
    _backend->configure_tcpserver(cfg.public_ip_address, _tcp_server.port());
  }

  downscaler::Downscaler& Server::downscaler()
  {
    return _downscaler;
  }

  void Server::run()
  {
    _downscaler.run();
    _http_server->run();
  }

  void Server::wait()
  {
    _downscaler.wait();
    _http_server->wait();
  }

  void Server::shutdown()
  {
    _downscaler.shutdown();
    _http_server->shutdown();
    _tcp_server.shutdown();
  }

} // namespace praas::control_plane
