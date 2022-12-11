
#include <praas/function/context.hpp>
#include <praas/process/invoker.hpp>

#include <signal.h>
#include <sys/time.h>
#include <execinfo.h>

#include <spdlog/spdlog.h>

#include "functions.hpp"
#include "opts.hpp"

praas::process::Invoker* instance = nullptr;

std::atomic<bool> ending{};

void signal_handler(int signal)
{
  spdlog::info("Handling signal {}", strsignal(signal));

  if(instance != nullptr) {
    instance->shutdown();
  }
  ending = true;
}

void failure_handler(int signum)
{
  if(instance != nullptr) {
    instance->shutdown();
  }

  fprintf(stderr, "Unfortunately, the invoker has crashed - signal %d.\n", signum);
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
  auto config = praas::process::opts(argc, argv);
  if (config.verbose)
    spdlog::set_level(spdlog::level::debug);
  else
    spdlog::set_level(spdlog::level::info);
  spdlog::set_pattern("[%H:%M:%S:%f] [P %P] [T %t] [%l] %v ");
  spdlog::info("Executing PraaS C++ invoker!");

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
    sigaction(SIGHUP, &sa, NULL);
  }

  praas::process::Invoker invoker{config.process_id, config.ipc_mode, config.ipc_name};
  praas::process::FunctionsLibrary library{config.code_location, config.code_config_location};
  instance = &invoker;

  praas::function::Context context = invoker.create_context();

  while(true) {

    auto invoc = invoker.poll();

    if(ending || !invoc.has_value()) {
      break;
    }

    auto& invoc_value = invoc.value();

    //spdlog::info("invoking {}, invocation key {}", invoc_value.function_name, invoc_value.key);
    auto func = library.get_function(invoc_value.function_name);
    if(!func) {
      spdlog::error("Could not load function {}", invoc_value.function_name);
    } else {
      context.start_invocation(invoc_value.key);
      int ret = (*func)(invoc_value, context);
      invoker.finish(context.invocation_id(), context.as_buffer(), ret);
      context.end_invocation();
      //spdlog::info("Finished invocation of {} with {}", invoc_value.function_name, invoc_value.key);
    }

  }

  spdlog::info("Process invoker is closing down");
  return 0;
}
