#ifndef PRAAS_CONTROL_PLANE_SERVER_HPP
#define PRAAS_CONTROL_PLANE_SERVER_HPP

#include <praas/control-plane/backend.hpp>
#include <praas/control-plane/deployment.hpp>
#include <praas/control-plane/http.hpp>
#include <praas/control-plane/resources.hpp>
#include <praas/control-plane/worker.hpp>

#include <spdlog/spdlog.h>

namespace praas::control_plane {

  struct Options {
    std::string config;
    bool verbose;
  };
  Options opts(int, char**);

  struct Server {

    Server(config::Config& cfg);

    void run();

    void shutdown();

    void wait();

    int http_port() const
    {
      return _http_server->port();
    }

    int tcp_port() const
    {
      return _tcp_server.port();
    }

  private:
    std::shared_ptr<spdlog::logger> _logger;

    Resources _resources;

    std::unique_ptr<deployment::Deployment> _deployment;

    std::unique_ptr<backend::Backend> _backend;

    worker::Workers _workers;

    tcpserver::TCPServer _tcp_server;

    // Shared pointer is required by drogon
    std::shared_ptr<HttpServer> _http_server;
  };

} // namespace praas::control_plane

#endif
