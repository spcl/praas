
#ifndef __CONTROLL_PLANE_SERVER_HPP__
#define __CONTROLL_PLANE_SERVER_HPP__

#include <sockpp/tcp_acceptor.h>

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
    bool _ending;

    Server(Options &);

    void start();
    void shutdown();
  };

}

#endif

