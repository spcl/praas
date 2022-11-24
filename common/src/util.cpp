#include <praas/common/util.hpp>

namespace praas::common::util {

  void traceback()
  {
    void* array[10];
    size_t size = backtrace(array, 10);
    char** trace = backtrace_symbols(array, size);
    for (size_t i = 0; i < size; ++i)
      spdlog::warn("Traceback {}: {}", i, trace[i]);
    free(trace);
  }

  bool expect_true(bool val)
  {
    if (!val) {
      spdlog::error("Expected true, got false, errno {}, message {}", errno, strerror(errno));
      traceback();
      exit(1);

      return false;
    }

    return true;
  }

  void assert_true(bool val)
  {
    if (!expect_true(val))
      exit(1);
  }
} // namespace praas::common::util
