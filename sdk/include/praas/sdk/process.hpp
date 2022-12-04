#ifndef PRAAS_SDK_PROCESS_HPP
#define PRAAS_SDK_PROCESS_HPP

#include <praas/sdk/invocation.hpp>

#include <sockpp/stream_socket.h>
#include <sockpp/tcp_connector.h>

namespace praas::sdk {

  struct Process {

    Process(const std::string& addr, int port);

    ~Process();

    bool connect();

    void disconnect();

    InvocationResult invoke(std::string_view function_name, std::string invocation_id, char* ptr, size_t len);

  private:

    sockpp::inet_address _addr;

    sockpp::tcp_connector _dataplane;

  };

}

#endif
