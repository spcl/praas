#ifndef PRAAS_PROCESS_INVOKER_OPTS_HPP
#define PRAAS_PROCESS_INVOKER_OPTS_HPP

#include <praas/process/ipc/ipc.hpp>

#include <string>

namespace praas::process {

  struct Options {

    ipc::IPCMode ipc_mode;
    std::string ipc_name;

    bool verbose;

  };

  Options opts(int argc, char ** argv);

}

#endif
