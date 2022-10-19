
#ifndef PRAAS_COMMON_EXCEPTIONS_HPP
#define PRAAS_COMMON_EXCEPTIONS_HPP

#include <stdexcept>

namespace praas::common {

  struct NotImplementedError : std::runtime_error {

    NotImplementedError(): std::runtime_error("Function is not implemented!");

  };

  struct ObjectExists : std::runtime_error {

    ObjectExists(std::string name) : std::runtime_error(name) {}
  };

  struct ObjectDoesNotExist : std::runtime_error {

    ObjectDoesNotExist(std::string name) : std::runtime_error(name) {}
  };

} // namespace praas::common

#endif
