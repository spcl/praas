#ifndef PRAAS_PROCESS_INVOKER_OPTS_HPP
#define PRAAS_PROCESS_INVOKER_OPTS_HPP

#include <praas/process/runtime/internal/ipc/ipc.hpp>

#include <string>

namespace praas::process {

  struct Options {

    std::string process_id;

    runtime::internal::ipc::IPCMode ipc_mode;
    std::string ipc_name;

    std::string code_location;
    std::string code_config_location;

    bool verbose;
  };

  Options opts(int argc, char** argv);

} // namespace praas::process

#endif
