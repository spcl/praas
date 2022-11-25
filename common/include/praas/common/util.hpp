#ifndef PRAAS_COMMON_UTIL_HPP
#define PRAAS_COMMON_UTIL_HPP

#include <praas/common/exceptions.hpp>

#include <execinfo.h>

#include <cereal/archives/json.hpp>
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

  template <typename T>
  void cereal_load_optional(cereal::JSONInputArchive& archive, const std::string& name, T& obj)
  {

    // Unfortunately, Cereal does not allow to skip non-existing objects easily.
    // There is also no separate exception type for this.
    try {
      archive(cereal::make_nvp(name, obj));
    } catch (cereal::Exception& exc) {

      // Catch non existing object
      if (std::string_view{exc.what()}.find(fmt::format("({}) not found", name)) !=
          std::string::npos) {

        archive.setNextName(nullptr);
        obj.set_defaults();

      } else {
        throw common::InvalidConfigurationError(
            "Could not parse HTTP configuration, reason: " + std::string{exc.what()}
        );
      }
    }
  }

} // namespace praas::common::util

#endif
