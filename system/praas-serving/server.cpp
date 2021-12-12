
#include <algorithm>

#include <spdlog/spdlog.h>

#include <praas/process.hpp>

#include "server.hpp"
#include "subprocess.hpp"
#include "../common/messages.hpp"

namespace praas::serving {


  Server::Server(Options & options):
    _processes(new std::optional<praas::serving::SubprocessFork>[options.processes]()),
    _hole_puncher_address(options.hole_puncher_address),
    _max_processes(options.processes),
    _port(options.port),
    _ending(false),
    _verbose(options.verbose),
    _enable_swapping(options.enable_swapping)
  {
    _listen.open(options.port);
  }

  Server::~Server()
  {
    delete[] _processes;
  }

  void Server::start()
  {
    if (!_listen)
      spdlog::error("Incorrect socket initialization! {}", _listen.last_error_str());

    spdlog::info("Local worker starts listening on port {}!", _port);
    while(!_ending) {
      sockpp::tcp_socket conn = _listen.accept();

      if(_ending)
        break;
      if (!conn)
        spdlog::error("Error accepting incoming connection: {}", _listen.last_error_str());

      // Start a new process
      praas::common::ProcessRequest req;
      ssize_t n = conn.read(req.data, req.EXPECTED_LENGTH);
      if(n == 0) {
        spdlog::debug("End of file on connection with {}.", conn.peer_address().to_string());
        conn.close();
        continue;
      }
      // Incorrect payload
      if(n != req.EXPECTED_LENGTH) {
        spdlog::error("Incorrect data size received {} from {}", n, conn.peer_address().to_string());
        int ret = 2;
        conn.write(&ret, sizeof(ret));
        conn.close();
        continue;
      }

      auto free_process = std::find_if(
        _processes, _processes + _max_processes,
        [](std::optional<praas::serving::SubprocessFork> & p) {
          return !p.has_value();
        }
      );
      int ret;
      if(free_process == _processes + _max_processes) {
        ret = 1;
        // 0 -> success, 1 -> exhausted resources, 2 -> another error
        conn.write(&ret, sizeof(ret));
        conn.close();
        spdlog::error("Achieved max number of processes! Can't scale more.");
      } else {

        auto pos = static_cast<int32_t>(std::distance(_processes, free_process));
        _processes[pos] = SubprocessFork();
        _processes[pos]->pos = pos;

        std::thread{&SubprocessFork::fork, &_processes[pos].value(), std::move(req), _hole_puncher_address, _enable_swapping}.detach();
        ret = 0;
        conn.write(&ret, sizeof(ret));
        conn.close();
      }
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
      }
    }
  }

}
