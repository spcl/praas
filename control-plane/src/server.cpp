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

  Server::Server(config::Config& cfg):
    _deployment(deployment::Deployment::construct(cfg)),
    _backend(backend::Backend::construct(cfg, *_deployment)),
    _workers(worker::Workers(cfg.workers, *_backend, *_deployment, _resources)),
    _tcp_server(tcpserver::TCPServer(cfg.tcpserver, _workers)),
    _http_server(std::make_shared<HttpServer>(cfg.http, _workers))
  {
    _logger = common::util::create_logger("Server");

    _workers.attach_tcpserver(_tcp_server);
    _backend->configure_tcpserver(cfg.public_ip_address, _tcp_server.port());
  }

  void Server::run()
  {
    _http_server->run();
  }

  void Server::wait()
  {
    _http_server->wait();
  }

  void Server::shutdown()
  {
    _http_server->shutdown();
    _tcp_server.shutdown();
  }

} // namespace praas::control_plane
