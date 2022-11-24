#ifndef PRAAS_FUNCTION_INVOCATION_HPP
#define PRAAS_FUNCTION_INVOCATION_HPP

#include <string>
#include <vector>

namespace praas::function {

  struct Messages {

    // FIXME: byte arguments
    void put(std::string name);
    void get(std::string name);

  };

}

#endif
