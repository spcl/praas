#ifndef PRAAS_PROCESS_RUNTIME_INVOCATION_HPP
#define PRAAS_PROCESS_RUNTIME_INVOCATION_HPP

#include <praas/process/runtime/buffer.hpp>

#include <string>
#include <vector>

namespace praas::process::runtime {

  struct Invocation {

    Invocation() = default;

    std::string key;

    std::string function_name;

    std::vector<Buffer> args;
  };

  struct InvocationResult {

    // InvocationResult() = default;

    std::string key;

    std::string function_name;

    int return_code;

    Buffer payload;
  };

} // namespace praas::process::runtime

#endif
