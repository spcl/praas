
#ifndef __LOCAL_WORKER_SERVER_HPP__
#define __LOCAL_WORKER_SERVER_HPP__

#include <sockpp/tcp_acceptor.h>
#include <thread>

namespace praas::process {
  struct Process;
}

namespace praas::local_worker {

  struct Options {
    int processes;
    int port;
    bool verbose;
  };
  Options opts(int, char**);

  struct Server
  {
    praas::process::Process** _processes;
    std::thread** _threads;
    int _max_processes;
    int _port;
    bool _ending;
    sockpp::tcp_acceptor _listen;

    Server(Options &);
    ~Server();

    void start();
    void shutdown();
  };

}

#endif

