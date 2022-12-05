#ifndef PRAAS_SDK_PROCESS_HPP
#define PRAAS_SDK_PROCESS_HPP

#include <praas/sdk/invocation.hpp>

#include <sockpp/stream_socket.h>
#include <sockpp/tcp_connector.h>

namespace praas::sdk {

  struct Process {

    Process() = default;

    Process(const std::string& addr, int port);

    Process(const Process &) = delete;
    Process(Process &&) = default;
    Process& operator=(const Process &) = delete;
    Process& operator=(Process &&) = default;

    ~Process();

    bool connect();

    void disconnect();

    InvocationResult invoke(std::string_view function_name, std::string invocation_id, char* ptr, size_t len);

    sockpp::tcp_connector& connection()
    {
      return _dataplane;
    }

  private:

    sockpp::inet_address _addr;

    sockpp::tcp_connector _dataplane;

  };

}

#endif
