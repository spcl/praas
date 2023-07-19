#ifndef PRAAS_PROCESS_CONTROLLER_CONFIG_HPP
#define PRAAS_PROCESS_CONTROLLER_CONFIG_HPP

#include <praas/process/runtime/internal/functions.hpp>
#include <praas/process/runtime/internal/ipc/ipc.hpp>

#include <istream>
#include <optional>
#include <string>

#include <cereal/archives/json.hpp>

namespace cereal {
  struct JSONInputArchive;
} // namespace cereal

namespace praas::process::config {

  struct Code {
    // FIXME: make it default - code location needs to have config.json file?
    static constexpr char DEFAULT_CODE_LOCATION[] = "/code";
    static constexpr char DEFAULT_CODE_CONFIG_LOCATION[] = "config.json";

    std::string location;
    std::string config_location;
    runtime::internal::Language language;

    std::string language_runtime_path;

    void load(cereal::JSONInputArchive& archive);
    void set_defaults();
    void load_env();
  };

  struct Controller {

    static constexpr int DEFAULT_PORT = 8080;
    static constexpr int DEFAULT_FUNCTION_WORKERS = 1;
    static constexpr int DEFAULT_MSG_SIZE = 8 * 1024;

    int port;
    bool verbose;
    int function_workers;
    runtime::internal::ipc::IPCMode ipc_mode;
    int ipc_message_size;
    std::string ipc_name_prefix;

    std::string deployment_location;

    std::string process_id;
    std::optional<std::string> control_plane_addr{};

    Code code;

    void load(cereal::JSONInputArchive& archive);
    void load_env();
    void set_defaults();

    static Controller deserialize(int argc, char** argv);
    static Controller deserialize(std::istream&);
  };

} // namespace praas::process::config

#endif
