
#include <praas/process/controller/controller.hpp>
#include <praas/process/controller/config.hpp>
#include <praas/process/controller/remote.hpp>

#include <spdlog/spdlog.h>

praas::process::Controller* instance = nullptr;


int main(int argc, char** argv)
{
  praas::process::config::Controller config;
  if(argc > 1) {
    config = praas::process::config::Controller::deserialize(argc, argv);
  } else {
    config.set_defaults();
  }
  config.load_env();

  if (config.verbose)
    spdlog::set_level(spdlog::level::debug);
  else
    spdlog::set_level(spdlog::level::info);
  spdlog::set_pattern("[%H:%M:%S:%f] [P %P] [T %t] [%l] %v ");
  spdlog::info("Executing PraaS controller!");

  praas::process::Controller controller{config};
  instance = &controller;

  char* swapin_loc = std::getenv("SWAPIN_LOCATION");
  if(swapin_loc) {
    // FIXME: async?
    // TODO: consider in future lazy loading
    if(!controller.swap_in(swapin_loc)) {
      controller.shutdown();
      return 1;
    }
  }

  praas::process::remote::TCPServer server{controller, config};
  controller.set_remote(&server);
  if(config.control_plane_addr.has_value()) {
    server.poll(config.control_plane_addr.value());
  } else {
    server.poll();
  }

  controller.start();

  spdlog::info("Process controller is closing down");
  return 0;
}
