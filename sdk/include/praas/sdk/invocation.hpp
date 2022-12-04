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
  };

}

#endif
