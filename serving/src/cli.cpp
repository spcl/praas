
#include <praas/serving/docker/server.hpp>

#include <chrono>
#include <climits>
#include <csignal>
#include <stdexcept>
#include <sys/time.h>
#include <thread>

#include <spdlog/spdlog.h>

std::shared_ptr<praas::serving::docker::HttpServer> instance = nullptr;

void signal_handler(int) // NOLINT
{
  assert(instance);
  instance->shutdown();
}

int main(int argc, char** argv)
{
  auto opts = praas::serving::docker::opts(argc, argv);
  if (opts.verbose) {
    spdlog::set_level(spdlog::level::debug);
  } else {
    spdlog::set_level(spdlog::level::info);
  }
  spdlog::set_pattern("[%H:%M:%S:%f] [P %P] [T %t] [%l] %v ");
  spdlog::info("Executing PraaS serving for Docker!");

  // Catch SIGINT
  struct sigaction sigIntHandler {};
  sigIntHandler.sa_handler = &signal_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, nullptr);

  auto server = std::make_shared<praas::serving::docker::HttpServer>(opts);
  instance = server;
  server->start();

  server->wait();

  spdlog::info("Docker serving is closing down");
  return 0;
}
