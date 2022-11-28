#ifndef PRAAS_FUNCTION_INVOCATION_HPP
#define PRAAS_FUNCTION_INVOCATION_HPP

#include <praas/function/buffer.hpp>

#include <string>
#include <vector>

namespace praas::function {

  struct Invocation {

    Invocation() = default;

    std::string key;

    std::string function_name;

    std::vector<praas::function::Buffer> args;

  };

}

#endif
