
#include <praas/process/controller/controller.hpp>
#include <praas/process/controller/config.hpp>
#include <praas/process/controller/remote.hpp>

#include <spdlog/spdlog.h>

praas::process::Controller* instance = nullptr;


int main(int argc, char** argv)
{
  auto config = praas::process::config::Controller::deserialize(argc, argv);
  if (config.verbose)
    spdlog::set_level(spdlog::level::debug);
  else
    spdlog::set_level(spdlog::level::info);
  spdlog::set_pattern("[%H:%M:%S:%f] [P %P] [T %t] [%l] %v ");
  spdlog::info("Executing PraaS controller!");

  praas::process::Controller controller{config};
  instance = &controller;

  praas::process::remote::TCPServer server{controller, config};
  controller.set_remote(&server);
  server.poll();

  controller.start();

  spdlog::info("Process controller is closing down");
  return 0;
}
