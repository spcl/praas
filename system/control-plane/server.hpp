
#ifndef __CONTROLL_PLANE_SERVER_HPP__
#define __CONTROLL_PLANE_SERVER_HPP__

#include <sockpp/tcp_acceptor.h>
#include <thread_pool.hpp>
#include <redis++.h>

#include "resources.hpp"
#include "backend.hpp"

namespace praas::control_plane {

  enum class FunctionBackendType {
    LOCAL = 0,
    AWS
  };

  struct Options {
    std::string redis_addr;
    int threads;
    std::string ip_address;
    int port;
    FunctionBackendType backend;
    std::string local_server;
    bool verbose;
  };
  Options opts(int, char**);

  struct Server
  {
    sockpp::tcp_acceptor _listen;
    thread_pool _pool;
    sw::redis::Redis _redis;
    Resources _resources;
    backend::Backend* _backend;
    bool _ending;

    Server(Options &);
    ~Server();

    void start();
    void shutdown();
  };

}

#endif

