#ifndef PRAAS_PROCESS_INVOKER_OPTS_HPP
#define PRAAS_PROCESS_INVOKER_OPTS_HPP

#include <praas/process/runtime/ipc/ipc.hpp>

#include <string>

namespace praas::process {

  struct Options {

    std::string process_id;

    runtime::ipc::IPCMode ipc_mode;
    std::string ipc_name;

    std::string code_location;
    std::string code_config_location;

    bool verbose;

  };

  Options opts(int argc, char ** argv);

}

#endif
