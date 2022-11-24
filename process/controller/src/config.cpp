
#include <praas/process/controller/config.hpp>

#include <cereal/archives/json.hpp>
#include <cereal/details/helpers.hpp>

#include <cxxopts.hpp>

#include <spdlog/spdlog.h>

#include <fstream>

namespace praas::process::config {

  void Controller::load(cereal::JSONInputArchive& archive)
  {
    archive(CEREAL_NVP(port));
    archive(CEREAL_NVP(verbose));
    archive(CEREAL_NVP(function_workers));

    std::string mode;
    archive(cereal::make_nvp("ipc-mode", mode));
    ipc_mode = ipc::deserialize(mode);
    archive(cereal::make_nvp("ipc-message-size", ipc_message_size));
  }

  void Controller::set_defaults()
  {
    port = DEFAULT_PORT;
    function_workers = DEFAULT_FUNCTION_WORKERS;
    verbose = false;
    ipc_mode = ipc::IPCMode::POSIX_MQ;
    ipc_message_size = DEFAULT_MSG_SIZE;
  }

  Controller Controller::deserialize(int argc, char** argv)
  {
    cxxopts::Options options(
        "praas-process-controller", "Executes PraaS functions and communication."
    );
    options.add_options()("c,config", "JSON config.", cxxopts::value<std::string>()->default_value(""));
    auto parsed_options = options.parse(argc, argv);

    std::string config_file{parsed_options["config"].as<std::string>()};

    Controller cfg;
    if(config_file.length() > 0) {
      std::ifstream in_stream{config_file};
      if (!in_stream.is_open()) {
        spdlog::error("Could not open config file {}", config_file);
        exit(1);
      }

      cereal::JSONInputArchive archive_in(in_stream);
      cfg.load(archive_in);
    } else {

      cfg.set_defaults();

    }

    return cfg;
  }

  Controller Controller::deserialize(std::istream & json_config)
  {

    Controller cfg;
    cereal::JSONInputArchive archive_in(json_config);
    cfg.load(archive_in);

    return cfg;
  }

} // namespace praas::process::config
