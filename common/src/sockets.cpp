
#include <praas/common/sockets.hpp>
#include <praas/common/util.hpp>

#include <netinet/in.h>
#include <netinet/tcp.h>

namespace praas::common::sockets {

  void disable_nagle(int fd)
  {
    int yes = 1;
    int result = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &yes, sizeof(int));

    util::assert_true(result >= 0);
  }

}
