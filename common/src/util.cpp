#include <praas/common/util.hpp>

#include <spdlog/sinks/stdout_color_sinks.h>

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

  std::shared_ptr<spdlog::logger> create_logger(std::string_view name)
  {
    auto sink = std::make_shared<spdlog::sinks::stderr_color_sink_st>();
    auto logger = std::make_shared<spdlog::logger>(std::string{name}, sink);
    logger->set_pattern("[%H:%M:%S:%f] [%n] [P %P] [T %t] [%l] %v ");
    return logger;
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
