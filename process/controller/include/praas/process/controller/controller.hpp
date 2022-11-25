#ifndef PRAAS_PROCESS_CONTROLLER_HPP
#define PRAAS_PROCESS_CONTROLLER_HPP

#include <praas/process/controller/config.hpp>
#include <praas/process/ipc/ipc.hpp>

#include <memory>

namespace praas::process {

  struct FunctionWorker {

    FunctionWorker(const char ** args, ipc::IPCMode, std::string ipc_name, int ipc_msg_size);

    ipc::IPCChannel& ipc_write();

    ipc::IPCChannel& ipc_read();

    int pid() const
    {
      return _pid;
    }

  private:
    std::unique_ptr<ipc::IPCChannel> _ipc_read;

    std::unique_ptr<ipc::IPCChannel> _ipc_write;

    int _pid;

    bool _busy;
  };

  struct Controller {

    // State

    // Messages

    // Swapper object

    // Poller object

    Controller(config::Controller cfg);

    ~Controller();

    void poll();

    void start();

    void shutdown();

  private:

    std::vector<FunctionWorker> _workers;

    int _worker_counter;

    int _epoll_fd;

    std::atomic<bool> _ending{};

    static constexpr int MAX_EPOLL_EVENTS = 32;
    static constexpr int EPOLL_TIMEOUT = 1000;
  };

}

#endif
