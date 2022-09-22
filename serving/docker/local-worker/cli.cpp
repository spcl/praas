
#include <chrono>
#include <stdexcept>
#include <thread>
#include <climits>
#include <sys/time.h>

#include <signal.h>

#include <spdlog/spdlog.h>

#include "server.hpp"

praas::local_worker::Server * instance = nullptr;

void signal_handler(int)
{
  assert(instance);
  instance->shutdown();
}

int main(int argc, char ** argv)
{
  auto opts = praas::local_worker::opts(argc, argv);
  if(opts.verbose)
    spdlog::set_level(spdlog::level::debug);
  else
    spdlog::set_level(spdlog::level::info);
  spdlog::set_pattern("[%H:%M:%S:%f] [P %P] [T %t] [%l] %v ");
  spdlog::info("Executing PraaS process worker!");

  // Catch SIGINT
  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = &signal_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, NULL);

  praas::local_worker::Server server{opts};
  instance = &server;
  server.start();

  spdlog::info("PraaS local worker is closing down");
  return 0;
}
