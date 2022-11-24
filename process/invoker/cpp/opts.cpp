
#include "opts.hpp"

#include <cxxopts.hpp>
#include <spdlog/spdlog.h>

namespace praas::process {

  Options opts(int argc, char ** argv)
  {
    cxxopts::Options options("praas-invoker-cpp", "Handle function invocations.");
    options.add_options()
      ("ipc-mode", "IPC mode.",  cxxopts::value<std::string>())
      ("ipc-name", "Name used to identify the IPC channel.",  cxxopts::value<std::string>())
      ("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"))
    ;
    auto parsed_options = options.parse(argc, argv);

    Options result;
    result.ipc_mode = ipc::deserialize(parsed_options["ipc-mode"].as<std::string>());
    if(result.ipc_mode == ipc::IPCMode::NONE) {
      spdlog::error("Incorrect IPC mode {} selected!", parsed_options["ipc-mode"].as<std::string>());
      exit(1);
    }
    result.ipc_name = parsed_options["ipc-name"].as<std::string>();
    result.verbose = parsed_options["verbose"].as<bool>();

    return result;
  }


}
