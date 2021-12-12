
#ifndef __SERVINGS_SERVER_HPP__
#define __SERVINGS_SERVER_HPP__

#include <thread>
#include <optional>

#include <sockpp/tcp_acceptor.h>

#include "subprocess.hpp"

namespace praas::process {
  struct Process;
}

namespace praas::serving {

  struct Options {
    std::string hole_puncher_address;
    int processes;
    int port;
    bool verbose;
    bool enable_swapping;
  };
  Options opts(int, char**);

  struct Server
  {
    std::optional<praas::serving::SubprocessFork>* _processes;
    std::string _hole_puncher_address;
    int _max_processes;
    int _port;
    bool _ending;
    bool _verbose;
    bool _enable_swapping;
    sockpp::tcp_acceptor _listen;

    Server(Options &);
    ~Server();

    void start();
    void handle_subprocess_shutdown();
    void shutdown();
  };

}

#endif

