
#include <algorithm>

#include <spdlog/spdlog.h>

#include <praas/process.hpp>

#include "server.hpp"
#include "request.hpp"

namespace praas::local_worker {

  Server::Server(Options & options):
    _processes(new praas::process::Process*[options.processes]()),
    _threads(new std::thread*[options.processes]()),
    _max_processes(options.processes),
    _port(options.port),
    _ending(false)
  {
    _listen.open(options.port);
  }

  Server::~Server()
  {
    for(int i = 0; i < _max_processes; ++i) {
      delete _processes[i];
      delete _threads[i];
    }
    delete[] _processes;
    delete[] _threads;
  }

  void Server::start()
  {
    if (!_listen)
      spdlog::error("Incorrect socket initialization! {}", _listen.last_error_str());

    Request req;
    while(!_ending) {
      sockpp::tcp_socket conn = _listen.accept();

      if(_ending)
        break;
      if (!conn)
        spdlog::error("Error accepting incoming connection: {}", _listen.last_error_str());

      // Start a new process
      ssize_t n = conn.read(req.buf, Request::REQUEST_BUF_SIZE);
      if(n != Request::REQUEST_BUF_SIZE) {
        spdlog::error("Incorrect data size received {}", n);
        int ret = 2;
        conn.write(&ret, sizeof(ret));
        conn.close();
      } else {

        auto it = std::find(_processes, _processes + _max_processes, nullptr);
        int ret;
        if(it == _processes + _max_processes) {
          ret = 1;
          spdlog::error("Achieved max number of processes! Can't scale more.");
        } else {
          // Allocate a new process
          ret = 0;
          auto pos = static_cast<int32_t>(std::distance(_processes, it));
          int port = _port + pos + 1;
          *it = new praas::process::Process{port, req.max_sessions()};
          _threads[pos] = new std::thread(&praas::process::Process::start, *it);

          spdlog::debug(
            "Allocate new process with max sessions {} on port {}",
            req.max_sessions(),
            port
          );
        }
        // 0 -> success, 1 -> exhausted resources
        conn.write(&ret, sizeof(ret));
        conn.close();
      }
    }

  }

  void Server::shutdown()
  {
    _ending = true;
    spdlog::info("Closing local worker.");
    _listen.shutdown();

    for(int i = 0; i < _max_processes; ++i) {
      spdlog::info("{} {} {}", i, fmt::ptr(_processes[i]), fmt::ptr(_threads[i]));
      if(_processes[i]) {
        _processes[i]->shutdown();
        spdlog::info("Join thread");
        _threads[i]->join();
      }
    }
  }

}
