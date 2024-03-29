#include <praas/process/controller/config.hpp>

#include <praas/common/util.hpp>

#include <cereal/archives/json.hpp>
#include <cereal/details/helpers.hpp>
#include <cxxopts.hpp>
#include <spdlog/spdlog.h>

#include <fstream>

namespace praas::process::config {

  void Code::load(cereal::JSONInputArchive& archive)
  {
    archive(cereal::make_nvp("location", this->location));
    archive(cereal::make_nvp("configuration-location", this->config_location));

    std::string language;
    archive(cereal::make_nvp("language", language));
    this->language = runtime::internal::string_to_language(language);
  }

  void Code::set_defaults()
  {
    location = DEFAULT_CODE_LOCATION;
    config_location = DEFAULT_CODE_CONFIG_LOCATION;
    language = runtime::internal::Language::CPP;
    language_runtime_path = "";
  }

  void Code::load_env()
  {
    char* env_val = getenv("CODE_LOCATION");
    if (env_val) {
      location = env_val;
    } else {
      spdlog::warn("Couldn't find environment variable CODE_LOCATION");
    }

    env_val = getenv("CONFIG_LOCATION");
    if (env_val) {
      config_location = env_val;
    } else {
      spdlog::warn("Couldn't find environment variable CONFIG_LOCATION");
    }
  }

  void Controller::load(cereal::JSONInputArchive& archive)
  {
    archive(CEREAL_NVP(port));
    archive(CEREAL_NVP(verbose));
    archive(CEREAL_NVP(function_workers));
    archive(CEREAL_NVP(process_id));

    std::string mode;
    archive(cereal::make_nvp("ipc-mode", mode));
    ipc_mode = runtime::internal::ipc::deserialize(mode);
    archive(cereal::make_nvp("ipc-message-size", ipc_message_size));

    archive(CEREAL_NVP(code));

    archive(CEREAL_NVP(process_id));
  }

  void Controller::load_env()
  {
    char* env_addr = getenv("CONTROLPLANE_ADDR");
    if (env_addr) {
      control_plane_addr = env_addr;
    } else {
      spdlog::warn("Couldn't find environment variable CONTROLPLANE_ADDR");
    }

    char* id_addr = getenv("PROCESS_ID");
    if (id_addr) {
      process_id = id_addr;
    } else {
      spdlog::warn("Couldn't find environment variable PROCESS_ID");
    }

    code.load_env();
  }

  void Controller::set_defaults()
  {
    port = DEFAULT_PORT;
    function_workers = DEFAULT_FUNCTION_WORKERS;
    verbose = false;
    ipc_mode = runtime::internal::ipc::IPCMode::POSIX_MQ;
    ipc_message_size = DEFAULT_MSG_SIZE;
    process_id = "TEST_PROCESS_ID";

    deployment_location = "";
    ipc_name_prefix = "";

    code.set_defaults();
  }

  Controller Controller::deserialize(int argc, char** argv)
  {
    cxxopts::Options options(
        "praas-process-controller", "Executes PraaS functions and communication."
    );
    options.add_options()(
        "c,config", "JSON config.", cxxopts::value<std::string>()->default_value("")
    );
    auto parsed_options = options.parse(argc, argv);

    std::string config_file{parsed_options["config"].as<std::string>()};

    Controller cfg;
    if (config_file.length() > 0) {
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

  Controller Controller::deserialize(std::istream& json_config)
  {

    Controller cfg;
    cereal::JSONInputArchive archive_in(json_config);
    cfg.load(archive_in);

    return cfg;
  }

} // namespace praas::process::config
