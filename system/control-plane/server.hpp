
#ifndef __CONTROLL_PLANE_SERVER_HPP__
#define __CONTROLL_PLANE_SERVER_HPP__

#include <sockpp/tcp_acceptor.h>
#include <thread_pool.hpp>

namespace praas::control_plane {

  struct Options {
    int threads;
    int port;
    bool verbose;
  };
  Options opts(int, char**);

  struct Server
  {
    sockpp::tcp_acceptor _listen;
    thread_pool _pool;
    bool _ending;

    Server(Options &);

    void start();
    void shutdown();
  };

}

#endif

