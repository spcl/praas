
#ifndef __CONTROLL_PLANE_WORKER_HPP__
#define __CONTROLL_PLANE_WORKER_HPP__

#include <sockpp/tcp_socket.h>

namespace praas::control_plane {

  void worker(sockpp::tcp_socket * conn)
  {
    char buf[16];
    ssize_t n = conn->read(buf, sizeof(buf));

    conn->write("hello client", 13);

    delete conn;
  }

}

#endif

