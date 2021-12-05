
#ifndef __LOCAL_WORKER_REQUEST_HPP__
#define __LOCAL_WORKER_REQUEST_HPP__

#include <cstdint>
#include <string>
#include <cstring>

namespace praas::local_worker {

  struct Request {

    // 2 bytes max_sessions
    // 4 bytes port
    // 15 bytes IP address
    static constexpr int REQUEST_BUF_SIZE = 23;
    int8_t buf[REQUEST_BUF_SIZE];

    Request()
    {
      memset(buf, 0, REQUEST_BUF_SIZE);
    }

    int16_t max_sessions()
    {
      return *reinterpret_cast<int16_t*>(buf);
    }

    int32_t port()
    {
      return *reinterpret_cast<int32_t*>(buf + 2);
    }

    std::string ip_address()
    {
      return std::string{reinterpret_cast<char*>(buf + 6), 15};
    }

    void fill(int16_t sessions, int32_t port, std::string ip_address)
    {
      *reinterpret_cast<int16_t*>(buf) = sessions;
      *reinterpret_cast<int32_t*>(buf + 2) = port;
      std::strncpy(reinterpret_cast<char*>(buf + 6), ip_address.data(), 15);
    }

  };

}

#endif

