#ifndef PRAAS_FUNCTION_INVOCATION_HPP
#define PRAAS_FUNCTION_INVOCATION_HPP

#include <praas/process/runtime/buffer.hpp>

#include <string>
#include <vector>

namespace praas::function {

  struct Invocation {

    std::string key;

    std::string function_name;

    std::vector<praas::process::runtime::Buffer<char>> args;

  };

}

#endif
