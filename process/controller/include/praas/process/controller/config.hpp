#ifndef PRAAS_PROCESS_CONTROLLER_CONFIG_HPP
#define PRAAS_PROCESS_CONTROLLER_CONFIG_HPP

#include <praas/process/runtime/ipc/ipc.hpp>
#include <praas/process/runtime/functions.hpp>

#include <istream>
#include <optional>
#include <string>

#include <cereal/archives/json.hpp>

namespace cereal {
  struct JSONInputArchive;
} // namespace cereal

namespace praas::process::config {

  struct Code {
    static constexpr char DEFAULT_CODE_LOCATION[] = "/code";
    static constexpr char DEFAULT_CODE_CONFIG_LOCATION[] = "config.json";

    std::string location;
    std::string config_location;
    runtime::functions::Language language;

    std::string language_runtime_path;

    void load(cereal::JSONInputArchive& archive);
    void set_defaults();
  };

  struct Controller {

    static constexpr int DEFAULT_PORT = 8080;
    static constexpr int DEFAULT_FUNCTION_WORKERS = 1;
    static constexpr int DEFAULT_MSG_SIZE = 8 * 1024;

    int port;
    bool verbose;
    int function_workers;
    runtime::ipc::IPCMode ipc_mode;
    int ipc_message_size;
    std::string ipc_name_prefix;

    std::string process_id;

    std::string deployment_location;

    Code code;

    void load(cereal::JSONInputArchive& archive);
    void set_defaults();

    static Controller deserialize(int argc, char** argv);
    static Controller deserialize(std::istream&);
  };

} // namespace praas::process::config

#endif
