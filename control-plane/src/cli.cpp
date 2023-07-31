#include <praas/common/http.hpp>
#include <praas/control-plane/server.hpp>

#include <praas/control-plane/config.hpp>

#include <chrono>
#include <climits>
#include <fstream>
#include <stdexcept>
#include <thread>

#include <csignal>
#include <sys/time.h>

#include <spdlog/spdlog.h>

praas::control_plane::Server* instance = nullptr;

void signal_handler(int /*unused*/)
{
  assert(instance);
  instance->shutdown();
}

int main(int argc, char** argv)
{
  auto cfg = praas::control_plane::config::Config::deserialize(argc, argv);
  if (cfg.verbose) {
    spdlog::set_level(spdlog::level::debug);
  } else {
    spdlog::set_level(spdlog::level::info);
  }
  spdlog::set_pattern("[%H:%M:%S:%f] [P %P] [T %t] [%l] %v ");
  spdlog::info("Executing PraaS control plane!");

  praas::common::http::HTTPClientFactory::initialize(cfg.http_client_io_threads);

  // Catch SIGINT
  struct sigaction sigIntHandler {};
  sigIntHandler.sa_handler = &signal_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, nullptr);

  praas::control_plane::Server server{cfg};
  instance = &server;
  server.run();

  server.wait();

  server.shutdown();

  spdlog::info("Control plane is closing down");
  return 0;
}
