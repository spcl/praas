
#ifndef PRAAS_COMMON_EXCEPTIONS_HPP
#define PRAAS_COMMON_EXCEPTIONS_HPP

#include <stdexcept>

namespace praas::common {

  struct PraaSException : std::runtime_error {

    PraaSException(const std::string& msg) : std::runtime_error(msg) {}
  };

  struct NotImplementedError : PraaSException {

    NotImplementedError() : PraaSException("Function is not implemented!") {}
  };

  struct InvalidConfigurationError : PraaSException {

    InvalidConfigurationError(const std::string& msg) : PraaSException(msg) {}
  };

  struct ObjectExists : PraaSException {

    ObjectExists(const std::string& name) : PraaSException(name) {}
  };

  struct ObjectDoesNotExist : PraaSException {

    ObjectDoesNotExist(const std::string& name) : PraaSException(name) {}
  };

  struct FailedAllocationError : PraaSException {

    FailedAllocationError(const std::string& name) : PraaSException(name) {}
  };

  struct InvalidProcessState : PraaSException {

    InvalidProcessState(const std::string& name) : PraaSException(name) {}
  };

} // namespace praas::common

#endif
