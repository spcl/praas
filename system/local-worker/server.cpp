
#include <algorithm>

#include <spdlog/spdlog.h>

#include <praas/process.hpp>

#include "server.hpp"
#include "../common/messages.hpp"

namespace praas::local_worker {

  Server::Server(Options & options):
    _processes(new std::optional<praas::process::Process>[options.processes]()),
    _threads(new std::thread*[options.processes]()),
    _hole_puncher_address(options.hole_puncher_address),
    _max_processes(options.processes),
    _port(options.port),
    _ending(false)
  {
    _listen.open(options.port);
  }

  Server::~Server()
  {
    for(int i = 0; i < _max_processes; ++i) {
      delete _threads[i];
    }
    delete[] _processes;
    delete[] _threads;
  }

  void Server::start()
  {
    if (!_listen)
      spdlog::error("Incorrect socket initialization! {}", _listen.last_error_str());

    spdlog::info("Local worker starts listening on port {}!", _port);
    praas::common::ProcessRequest req;
    while(!_ending) {
      sockpp::tcp_socket conn = _listen.accept();

      if(_ending)
        break;
      if (!conn)
        spdlog::error("Error accepting incoming connection: {}", _listen.last_error_str());

      // Start a new process
      ssize_t n = conn.read(req.data, req.REQUEST_BUF_SIZE);
      if(n == 0) {
        spdlog::debug("End of file on connection with {}.", conn.peer_address().to_string());
        conn.close();
        continue;
      }
      // Incorrect payload
      if(n != req.REQUEST_BUF_SIZE) {
        spdlog::error("Incorrect data size received {} from {}", n, conn.peer_address().to_string());
        int ret = 2;
        conn.write(&ret, sizeof(ret));
        conn.close();
        continue;
      }

      auto* free_process = std::find_if(
        _processes, _processes + _max_processes,
        [](std::optional<praas::process::Process> & p) {
          return !p.has_value();
        }
      );
      int ret;
      if(free_process == _processes + _max_processes) {
        ret = 1;
        spdlog::error("Achieved max number of processes! Can't scale more.");
      } else {
        // Allocate a new process
        auto pos = static_cast<int32_t>(std::distance(_processes, free_process));
        *free_process = praas::process::Process::create(
          req.process_id(), req.ip_address(), req.port(), _hole_puncher_address, req.max_sessions()
        );
        if(free_process->has_value()) {
          _threads[pos] = new std::thread(&praas::process::Process::start, &free_process->value());
          spdlog::debug(
            "Allocate new process {} with max sessions {}, connect to {}:{}",
            req.process_id(),
            req.max_sessions(),
            req.ip_address(),
            req.port()
          );
          ret = 0;
        } else {
          spdlog::error(
            "Failed to allocate a new process {} with max sessions {}, connect to {}:{}",
            req.process_id(),
            req.max_sessions(),
            req.ip_address(),
            req.port()
          );
          ret = 2;
        }
      }
      // 0 -> success, 1 -> exhausted resources, 2 -> another error
      conn.write(&ret, sizeof(ret));
      conn.close();
    }
    spdlog::error("Local worker stops listening!");
  }

  void Server::shutdown()
  {
    _ending = true;
    spdlog::info("Closing local worker.");
    _listen.shutdown();

    for(int i = 0; i < _max_processes; ++i) {
      if(_processes[i]) {
        _processes[i]->shutdown();
        _threads[i]->join();
      }
    }
  }

}
