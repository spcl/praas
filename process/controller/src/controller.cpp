
#include <praas/process/controller/controller.hpp>

#include <praas/common/exceptions.hpp>
#include <praas/common/util.hpp>
#include <praas/process/ipc/ipc.hpp>
#include <praas/process/ipc/messages.hpp>

#include <memory>

#include <sys/epoll.h>
#include <sys/eventfd.h>

namespace praas::process {

  namespace {

    // This assumes that the pointer to the socket does NOT change after
    // submitting to epoll.
    template <typename T>
    bool epoll_apply(int epoll_fd, int fd, T* data, uint32_t epoll_events, int flags)
    {
      spdlog::debug(
          "Adding to epoll connection, fd {}, ptr {}, events {}", fd,
          // NOLINTNEXTLINE
          fmt::ptr(reinterpret_cast<void*>(data)), epoll_events
      );

      epoll_event event{};
      memset(&event, 0, sizeof(epoll_event));
      event.events = epoll_events;
      // NOLINTNEXTLINE
      event.data.ptr = reinterpret_cast<void*>(data);

      if (epoll_ctl(epoll_fd, flags, fd, &event) == -1) {
        spdlog::error("Adding socket to epoll failed, reason: {}", strerror(errno));
        return false;
      }
      return true;
    }

    template <typename T>
    bool epoll_add(int epoll_fd, int fd, T* data, uint32_t epoll_events)
    {
      return epoll_apply(epoll_fd, fd, data, epoll_events, EPOLL_CTL_ADD);
    }

    template <typename T>
    bool epoll_mod(int epoll_fd, int fd, T* data, uint32_t epoll_events)
    {
      return epoll_apply(epoll_fd, fd, data, epoll_events, EPOLL_CTL_MOD);
    }
  } // namespace

  FunctionWorker::FunctionWorker(
      const char** args, ipc::IPCMode mode, std::string ipc_name, int ipc_msg_size
  )
  {
    int mypid = fork();
    if (mypid < 0) {
      throw praas::common::PraaSException{fmt::format("Fork failed! {}", mypid)};
    }

    if (mypid == 0) {

      mypid = getpid();
      auto out_file = ("invoker_" + std::to_string(mypid));

      spdlog::info("Invoker begins work on PID {}", mypid);
      int fd = open(out_file.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
      dup2(fd, 1);
      dup2(fd, 2);
      int ret = execvp(args[0], const_cast<char**>(&args[0]));
      if (ret == -1) {
        spdlog::error("Invoker process failed {}, reason {}", errno, strerror(errno));
        close(fd);
        exit(1);
      }

    } else {
      spdlog::info("Started invoker process with PID {}", mypid);
    }

    _pid = mypid;

    if (mode == ipc::IPCMode::POSIX_MQ) {
      _ipc = std::make_unique<ipc::POSIXMQChannel>(ipc_name, ipc_msg_size, true);
    }
  }

  ipc::IPCChannel& FunctionWorker::ipc()
  {
    return *_ipc;
  }

  Controller::Controller(config::Controller cfg) : _worker_counter(0)
  {

    for (size_t i = 0; i < cfg.function_workers; ++i) {

      std::string ipc_name = "/praas_queue_" + std::to_string(_worker_counter++);

      // FIXME: enable Python
      const char* argv[] = {
          "process/bin/cpp_invoker_exe",
          "--ipc-mode",
          "posix_mq",
          "--ipc-name",
          ipc_name.c_str(),
          nullptr};

      _workers.emplace_back(argv, cfg.ipc_mode, ipc_name, cfg.ipc_message_size);
    }

    // size is ignored by Linux
    _epoll_fd = epoll_create(255);
    if (_epoll_fd < 0) {
      throw praas::common::PraaSException(
          fmt::format("Incorrect epoll initialization! {}", strerror(errno))
      );
    }

    // FIXME: other IPC methods
    for (FunctionWorker& worker : _workers) {
      common::util::assert_true(
        epoll_add(_epoll_fd, worker.ipc().fd(), &worker, EPOLLIN | EPOLLPRI)
      );
    }
  }

  Controller::~Controller() {}

  void Controller::poll()
  {
    // customize
    ipc::BufferQueue<char> buffers(10, 1024);

    std::array<epoll_event, MAX_EPOLL_EVENTS> events;
    while (true) {

      // FIXME: REMOVE
      for(int i = 0; i < _workers.size(); ++i) {
        ipc::GetRequest req;
        auto buf = buffers.retrieve_buffer(300);
        buf.len = 10;
        _workers[i].ipc().send(req, {buf});
        buffers.return_buffer(buf);
      }

      int events_count = epoll_wait(_epoll_fd, events.data(), MAX_EPOLL_EVENTS, EPOLL_TIMEOUT);

      // Finish if we failed (but we were not interrupted), or when end was requested.
      if (_ending || (events_count == -1 && errno != EINVAL)) {
        break;
      }

      for (int i = 0; i < events_count; ++i) {

      }
    }

    spdlog::info("Controller finished polling");
  }

  void Controller::start()
  {
    poll();
  }

  void Controller::shutdown()
  {
    _ending = true;
    spdlog::info("Closing controller polling.");
  }

} // namespace praas::process
