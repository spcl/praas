
#ifndef __SERVINGS_SUBPROCESS_HPP__
#define __SERVINGS_SUBPROCESS_HPP__

#include <sockpp/tcp_connector.h>
#include <thread>
#include <optional>

#include <sockpp/tcp_connector.h>

#include "../common/messages.hpp"

namespace praas::process {
  struct Process;
}

namespace praas::serving {

  struct SubprocessFork {

    int pos;
    int pid;

    void fork(praas::common::ProcessRequest req, std::string, bool, bool);
    void shutdown();
  };

}

#endif

