#ifndef PRAAS_PROCESS_CONTROLLER_REMOTE_HPP
#define PRAAS_PROCESS_CONTROLLER_REMOTE_HPP

#include <praas/process/runtime/buffer.hpp>

#include <optional>
#include <string>
#include <unordered_map>

namespace praas::process::remote {

  struct Server {

    Server() = default;
    Server(const Server&) = default;
    Server(Server&&) = delete;
    Server& operator=(const Server&) = default;
    Server& operator=(Server&&) = delete;
    virtual ~Server() = default;

    virtual void poll() = 0;

    // Receive messages to be sent by the process controller.
    // Includes results of invocations and put requests.
    virtual void invocation_result(
        const std::optional<std::string>& remote_process,
        std::string_view invocation_id,
        int return_code,
        runtime::Buffer<char> payload
    ) = 0;
    virtual void put_message() = 0;

  };

  struct TCPServer : Server {


  };

} // namespace praas::process::remote

#endif
