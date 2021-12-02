
#ifndef __CONTROLL_PLANE_SERVER_HPP__
#define __CONTROLL_PLANE_SERVER_HPP__

#include <sockpp/tcp_acceptor.h>
#include <thread_pool.hpp>
#include <redis++.h>

namespace praas::control_plane {

  struct Options {
    std::string redis_addr;
    int threads;
    int port;
    bool verbose;
  };
  Options opts(int, char**);

  struct Server
  {
    sockpp::tcp_acceptor _listen;
    thread_pool _pool;
    sw::redis::Redis* _redis;
    bool _ending;

    Server(Options &);
    ~Server();

    void start();
    void shutdown();
  };

}

#endif

