
#ifndef __CONTROLL_PLANE_WORKER_HPP__
#define __CONTROLL_PLANE_WORKER_HPP__

#include <spdlog/spdlog.h>
#include <sockpp/tcp_socket.h>
#include <redis++.h>

namespace praas::control_plane {

  void worker(sockpp::tcp_socket * conn, sw::redis::Redis* redis_conn)
  {
    char buf[16];
    ssize_t n = conn->read(buf, sizeof(buf));

    auto val = redis_conn->get("val");
    if(val)
      spdlog::info(val.value());
    else
      spdlog::info("none");
    redis_conn->set("val", "new");

    conn->write("hello client", 13);

    delete conn;
  }

}

#endif

