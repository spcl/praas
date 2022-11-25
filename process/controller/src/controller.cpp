
#include <praas/process/controller/config.hpp>
#include <praas/process/controller/controller.hpp>

#include <praas/common/exceptions.hpp>
#include <praas/common/util.hpp>
#include <praas/process/ipc/ipc.hpp>
#include <praas/process/ipc/messages.hpp>

#include <filesystem>
#include <fstream>
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
      _ipc_read = std::make_unique<ipc::POSIXMQChannel>(
          ipc_name + "_read", ipc::IPCDirection::READ, true, ipc_msg_size
      );
      _ipc_write = std::make_unique<ipc::POSIXMQChannel>(
          ipc_name + "_write", ipc::IPCDirection::WRITE, true, ipc_msg_size
      );
    }
  }

  Controller::Controller(config::Controller cfg):
    _workers(cfg)
  {
    auto path = std::filesystem::path{cfg.code.location} / cfg.code.config_location;
    spdlog::debug("Loading function configurationf rom {}", path.c_str());
    std::ifstream in_stream{path};
    if(!in_stream.is_open()) {
      throw praas::common::PraaSException{fmt::format("Could not find file {}", path.c_str())};
    }
    _work_queue.initialize(in_stream, cfg.code.language);

    // size is ignored by Linux
    _epoll_fd = epoll_create(255);
    if (_epoll_fd < 0) {
      throw praas::common::PraaSException(
          fmt::format("Incorrect epoll initialization! {}", strerror(errno))
      );
    }

    // FIXME: other IPC methods
    for (FunctionWorker& worker : _workers.workers()) {
      common::util::assert_true(
          epoll_add(_epoll_fd, worker.ipc_read().fd(), &worker, EPOLLIN | EPOLLPRI)
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

      int events_count = epoll_wait(_epoll_fd, events.data(), MAX_EPOLL_EVENTS, EPOLL_TIMEOUT);

      // Finish if we failed (but we were not interrupted), or when end was requested.
      if (_ending || (events_count == -1 && errno != EINVAL)) {
        break;
      }

      std::cerr << "Events: " << events_count << std::endl;
      for (int i = 0; i < events_count; ++i) {
        spdlog::error(
            "{} {} ", events[i].events, static_cast<FunctionWorker*>(events[i].data.ptr)->pid()
        );

        // FIXME: Received invocation request
        // add this to the work queue

        // FIXME: received completion from worker
        // mark as done and add

        // FIXME: received get-put
      }

      // walk over all functions in a queue, schedule whatever possible
      while(_workers.has_idle_workers()) {

        Invocation* invoc = _work_queue.next();

        if(!invoc) {
          break;
        }

        // schedule on an idle worker
        _workers.submit(*invoc);

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