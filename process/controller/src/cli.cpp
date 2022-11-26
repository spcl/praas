
#include <praas/process/controller/controller.hpp>
#include <praas/process/controller/config.hpp>

#include <signal.h>
#include <sys/time.h>
#include <execinfo.h>

#include <spdlog/spdlog.h>

praas::process::Controller* instance = nullptr;

void signal_handler(int)
{
  assert(instance);
  instance->shutdown();
}

void failure_handler(int signum)
{
  fprintf(stderr, "Unfortunately, the process has crashed - signal %d.\n", signum);
  void *array[10];
  size_t size;
  // get void*'s for all entries on the stack
  size = backtrace(array, 10);
  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", signum);
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1);
}

int main(int argc, char** argv)
{
  auto config = praas::process::config::Controller::deserialize(argc, argv);
  if (config.verbose)
    spdlog::set_level(spdlog::level::debug);
  else
    spdlog::set_level(spdlog::level::info);
  spdlog::set_pattern("[%H:%M:%S:%f] [P %P] [T %t] [%l] %v ");
  spdlog::info("Executing PraaS controller!");

  {
    // Catch SIGINT
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = &signal_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);
  }

  {
    // Catch falure signals
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = failure_handler;
    sa.sa_flags   = SA_SIGINFO;

    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGPIPE, &sa, NULL);
  }

  praas::process::Controller controller{config};
  instance = &controller;
  controller.start();

  spdlog::info("Process controller is closing down");
  return 0;
}
