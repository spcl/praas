#ifndef PRAAS_SDK_PROCESS_HPP
#define PRAAS_SDK_PROCESS_HPP

#include <praas/common/messages.hpp>
#include <praas/sdk/invocation.hpp>

#include <sockpp/stream_socket.h>
#include <sockpp/tcp_connector.h>

namespace praas::sdk {

  struct Process {

    Process() = default;

    Process(std::string app, std::string pid, const std::string& addr, int port, bool disable_nagle = true);

    Process(const Process &) = delete;
    Process(Process &&) = default;
    Process& operator=(const Process &) = delete;
    Process& operator=(Process &&) = default;

    ~Process();

    bool connect();

    void disconnect();

    bool is_alive();

    InvocationResult invoke(std::string_view function_name, std::string invocation_id, char* ptr, size_t len);

    sockpp::tcp_connector& connection()
    {
      return _dataplane;
    }

    const std::string app_name;

    const std::string process_id;

  private:

    praas::common::message::MessageData _response;

    bool _disable_nagle = true;

    bool _disconnected = true;

    sockpp::inet_address _addr;

    sockpp::tcp_connector _dataplane;

  };

}

#endif
