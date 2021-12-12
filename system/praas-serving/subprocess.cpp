
#include <fcntl.h>

#include <spdlog/spdlog.h>

#include "subprocess.hpp"

namespace praas::serving {


  void SubprocessFork::fork(praas::common::ProcessRequest req, std::string hole_puncher_address, bool enable_swapping, bool use_docker)
  {
    spdlog::debug(
      "Allocate new process {} with max sessions {}, connect to {}:{}",
      req.process_id(),
      req.max_sessions(),
      req.ip_address(),
      req.port()
    );

    // FIXME: here we should have a fork of process with restricted permissions
    pid = vfork();
    if(pid == 0) {
      auto out_file = ("subprocess_" + req.process_id());
      int fd = open(out_file.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
      dup2(fd, 1);
      dup2(fd, 2);
      // Take memory from shm object -> it's in bytes now and definetely positive
      std::string max_sessions_str = std::to_string(req.max_sessions());
      std::string enable_swapping_str = std::to_string(enable_swapping);

      char addr[40];
      memset(addr, 0, sizeof(char) * 40);
      strcat(addr, req.ip_address().c_str());
      strcat(addr, ":");
      strcat(addr, std::to_string(req.port()).c_str());

      if(use_docker) {
        std::string process_id = req.process_id();
        // FIXME: do not hardcode this
        const char * argv[] = {
          "/usr/bin/docker", "run",
          "praas/runtime",
          "/dev-praas/bin/process_exec",
          "--process-id", process_id.c_str(),
          "--control-plane-address", addr,
          "--hole-puncher-address", hole_puncher_address.c_str(),
          "--max-sessions", max_sessions_str.c_str(),
          "--enable-swapping", enable_swapping_str.c_str(),
          "-v", nullptr
        };
        for(int i = 0; i < 15;++i)
          spdlog::debug(argv[i]);

        int ret = execv(argv[0], const_cast<char**>(&argv[0]));
        if(ret == -1) {
          spdlog::error("Executor process failed {}, reason {}", errno, strerror(errno));
          exit(1);
        }
      } else {
        std::string process_id = req.process_id();
        const char * argv[] = {
          "/dev-praas/bin/process_exec",
          "--process-id", process_id.c_str(),
          "--control-plane-address", addr,
          "--hole-puncher-address", hole_puncher_address.c_str(),
          "--max-sessions", max_sessions_str.c_str(),
          "--enable-swapping", enable_swapping_str.c_str(),
          "-v", nullptr
        };

        int ret = execv(argv[0], const_cast<char**>(&argv[0]));
        if(ret == -1) {
          spdlog::error("Executor process failed {}, reason {}", errno, strerror(errno));
          exit(1);
        }
      }
    } else {
      spdlog::debug("Launched subprocess executor for subprocess {}, PID {}", req.process_id(), pid);
      return;
    }
    // Avoid compiler errors
    return;
  }

  void SubprocessFork::shutdown()
  {
    // FIXME: send kill signals
  }

}
