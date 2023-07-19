
#include "opts.hpp"

#include <cxxopts.hpp>
#include <spdlog/spdlog.h>

namespace praas::process {

  Options opts(int argc, char** argv)
  {
    cxxopts::Options options("praas-invoker-cpp", "Handle function invocations.");
    options
        .add_options()("process-id", "Process identificator.", cxxopts::value<std::string>())("ipc-mode", "IPC mode.", cxxopts::value<std::string>())("ipc-name", "Name used to identify the IPC channel.", cxxopts::value<std::string>())("code-location", "Location of functions.", cxxopts::value<std::string>())("code-config-location", "Name of the function configuration.", cxxopts::value<std::string>())(
            "v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false")
        );
    auto parsed_options = options.parse(argc, argv);

    Options result;
    result.process_id = parsed_options["process-id"].as<std::string>();
    result.ipc_mode =
        runtime::internal::ipc::deserialize(parsed_options["ipc-mode"].as<std::string>());
    if (result.ipc_mode == runtime::internal::ipc::IPCMode::NONE) {
      spdlog::error(
          "Incorrect IPC mode {} selected!", parsed_options["ipc-mode"].as<std::string>()
      );
      exit(1);
    }
    result.ipc_name = parsed_options["ipc-name"].as<std::string>();
    result.verbose = parsed_options["verbose"].as<bool>();

    result.code_location = parsed_options["code-location"].as<std::string>();
    result.code_config_location = parsed_options["code-config-location"].as<std::string>();

    return result;
  }

} // namespace praas::process
