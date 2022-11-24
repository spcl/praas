
#ifndef PRAAS__CONTROLL_PLANE_CONFIG_HPP
#define PRAAS__CONTROLL_PLANE_CONFIG_HPP

#include <praas/process/ipc/ipc.hpp>

#include <optional>
#include <string>
#include <istream>

#include <cereal/archives/json.hpp>

namespace cereal {
  struct JSONInputArchive;
} // namespace cereal

namespace praas::process::config {

  struct Controller {

    static constexpr int DEFAULT_PORT = 8080;
    static constexpr int DEFAULT_FUNCTION_WORKERS = 1;
    static constexpr int DEFAULT_MSG_SIZE = 8 * 1024;

    int port;
    bool verbose;
    int function_workers;
    ipc::IPCMode ipc_mode;
    int ipc_message_size;

    void load(cereal::JSONInputArchive& archive);
    void set_defaults();

    static Controller deserialize(int argc, char ** argv);
    static Controller deserialize(std::istream &);
  };

} // namespace praas::process::config

#endif
