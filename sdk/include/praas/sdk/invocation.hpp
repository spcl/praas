#ifndef PRAAS_SDK_INVOCATION_HPP
#define PRAAS_SDK_INVOCATION_HPP

#include <memory>
#include <string>
#include <vector>

namespace praas::sdk {

  struct InvocationResult {

    int return_code;

    std::unique_ptr<char[]> payload;

    size_t payload_len;

    std::string error_message{};
  };

  struct ControlPlaneInvocationResult {

    std::string invocation_id;

    int return_code;

    std::string response;
  };

} // namespace praas::sdk

#endif
