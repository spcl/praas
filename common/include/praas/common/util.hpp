#ifndef PRAAS_COMMON_UTIL_HPP
#define PRAAS_COMMON_UTIL_HPP

#include <execinfo.h>

#include <spdlog/spdlog.h>

namespace praas::common::util {

  void traceback();

  bool expect_true(bool val);
  void assert_true(bool val);

  template <typename U>
  bool expect_zero(U&& u)
  {
    if (u) {
      spdlog::error("Expected zero, found: {}, errno {}, message {}", u, errno, strerror(errno));
      traceback();
      return false;
    }
    return true;
  }

  template <typename U>
  bool expect_other(U&& u, int val)
  {
    if (u == val) {
      spdlog::error(
          "Expected value other than {}, found: {}, errno {}, message {}", val, u, errno,
          strerror(errno)
      );
      traceback();
      return false;
    }
    return true;
  }

  template <typename U>
  void assert_zero(U&& u)
  {
    if (!expect_zero(u))
      exit(1);
  }

  template <typename U>
  void assert_other(U&& u, int val)
  {
    if (!expect_other(u, val))
      exit(1);
  }
} // namespace praas::common::util

#endif
