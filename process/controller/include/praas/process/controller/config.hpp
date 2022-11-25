
#ifndef PRAAS__CONTROLL_PLANE_CONFIG_HPP
#define PRAAS__CONTROLL_PLANE_CONFIG_HPP

#include <praas/process/ipc/ipc.hpp>

#include <istream>
#include <optional>
#include <string>

#include <cereal/archives/json.hpp>

namespace cereal {
  struct JSONInputArchive;
} // namespace cereal

namespace praas::process::config {

  enum class Language { CPP = 0, PYTHON, NONE };

  std::string language_to_string(Language val);

  Language string_to_language(std::string language);

  struct Code {
    static constexpr char DEFAULT_CODE_LOCATION[] = "/code";
    static constexpr char DEFAULT_CODE_CONFIG_LOCATION[] = "config.json";

    std::string location;
    std::string config_location;
    Language language;

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
    ipc::IPCMode ipc_mode;
    int ipc_message_size;

    Code code;

    void load(cereal::JSONInputArchive& archive);
    void set_defaults();

    static Controller deserialize(int argc, char** argv);
    static Controller deserialize(std::istream&);
  };

} // namespace praas::process::config

#endif
