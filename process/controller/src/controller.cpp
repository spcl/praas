
#include <praas/process/controller/controller.hpp>

#include <praas/common/exceptions.hpp>
#include <praas/common/messages.hpp>
#include <praas/common/util.hpp>
#include <praas/process/controller/config.hpp>
#include <praas/process/controller/remote.hpp>
#include <praas/process/runtime/ipc/messages.hpp>

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

  Controller::Controller(config::Controller cfg)
      : _buffers(DEFAULT_BUFFER_MESSAGES, DEFAULT_BUFFER_SIZE), _workers(cfg),
        _work_queue(_functions)
  {
    auto path = std::filesystem::path{cfg.code.location} / cfg.code.config_location;
    spdlog::debug("Loading function configurationf rom {}", path.c_str());
    std::ifstream in_stream{path};
    if (!in_stream.is_open()) {
      throw praas::common::PraaSException{fmt::format("Could not find file {}", path.c_str())};
    }
    _functions.initialize(in_stream, cfg.code.language);

    // size is ignored by Linux
    _epoll_fd = epoll_create(255);
    if (_epoll_fd < 0) {
      throw praas::common::PraaSException(
          fmt::format("Incorrect epoll initialization! {}", strerror(errno))
      );
    }

    // Use for notification
    common::util::assert_other(_event_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK), -1);
    common::util::assert_true(epoll_add(_epoll_fd, _event_fd, this, EPOLLIN | EPOLLET | EPOLLPRI));

    // FIXME: other IPC methods
    for (FunctionWorker& worker : _workers.workers()) {
      common::util::assert_true(
          epoll_add(_epoll_fd, worker.ipc_read().fd(), &worker, EPOLLIN | EPOLLPRI)
      );
    }
  }

  Controller::~Controller()
  {
    if(!_ending)
      shutdown();
    close(_event_fd);
    close(_epoll_fd);
  }

  void Controller::set_remote(remote::Server* server)
  {
    this->_server = server;
  }

  void Controller::remote_message(
      praas::common::message::Message&& msg, runtime::Buffer<char>&& payload, std::string process_id
  )
  {
    _external_queue.emplace(process_id, msg, payload);
    uint64_t tmp = 1;
    std::cerr << "Write to " << _event_fd << std::endl;
    common::util::assert_other(write(_event_fd, &tmp, sizeof(tmp)), -1);
  }

  void Controller::dataplane_message(
      praas::common::message::Message&& msg, runtime::Buffer<char>&& payload
  )
  {
    _external_queue.emplace(std::nullopt, msg, payload);
    uint64_t tmp = 1;
    std::cerr << "Write to " << _event_fd << std::endl;
    common::util::assert_other(write(_event_fd, &tmp, sizeof(tmp)), -1);
  }

  void Controller::_process_external_message(ExternalMessage& msg)
  {
    auto parsed_msg = msg.msg.parse();

    std::visit(
        runtime::ipc::overloaded{
            [&, this](common::message::InvocationRequestParsed& req) mutable {
              spdlog::info(
                  "Received invocation request of {}, inputs {}", req.function_name(),
                  req.payload_size()
              );
              _work_queue.add_payload(
                  std::string{req.function_name()}, std::string{req.invocation_id()},
                  std::move(msg.payload),
                  (msg.source.has_value() ? InvocationSource::from_process(msg.source.value())
                                          : InvocationSource::from_dataplane())
              );
            },
            [](auto&) { spdlog::error("Received unsupported message!"); }},
        parsed_msg
    );
  }

  void Controller::_process_internal_message(
      FunctionWorker& worker, const runtime::ipc::Message& msg, runtime::Buffer<char>&& payload
  )
  {
    auto parsed_msg = msg.parse();

    // FIXME: invocation request
    // FIXME: put
    // FIXME: get
    std::visit(
        runtime::ipc::overloaded{
            [&, this](runtime::ipc::InvocationResultParsed& req) mutable {
              spdlog::info(
                  "Received invocation result of {}, status {}, output size {}",
                  req.invocation_id(), req.status_code(), payload.len
              );

              std::optional<Invocation> invoc =
                  _work_queue.finish(std::string{req.invocation_id()});
              if (invoc.has_value()) {

                Invocation& invocation = invoc.value();

                // FIXME: send to the tcp server
                // FIXME: check work queue for the source
                if (invocation.source.is_remote()) {

                  _server->invocation_result(
                    invocation.source.remote_process,
                    invocation.req.invocation_id(),
                    payload
                  );
                }

              } else {
                spdlog::error("Could not find invocation for ID {}", req.invocation_id());
              }
              _workers.finish(worker);
            },
            [](auto&) { spdlog::error("Received unsupported message!"); }},
        parsed_msg
    );
  }

  void Controller::poll()
  {
    // FIXME: customize
    runtime::BufferQueue<char> buffers(10, 1024);
    ExternalMessage msg;

    std::array<epoll_event, MAX_EPOLL_EVENTS> events;
    while (true) {

      int events_count = epoll_wait(_epoll_fd, events.data(), MAX_EPOLL_EVENTS, EPOLL_TIMEOUT);
      std::cerr << events_count << std::endl;

      // Finish if we failed (but we were not interrupted), or when end was requested.
      if (_ending || (events_count == -1 && errno != EINVAL)) {
        break;
      }

      std::cerr << "Events: " << events_count << std::endl;
      for (int i = 0; i < events_count; ++i) {

        // Wake-up signal
        if (events[i].data.ptr == this) {

          spdlog::debug("Wake-up with external message");

          uint64_t read_val;
          read(_event_fd, &read_val, sizeof(read_val));

          while (_external_queue.try_pop(msg)) {
            _process_external_message(msg);
          }
        }
        // Message
        else {

          FunctionWorker& worker = *static_cast<FunctionWorker*>(events[i].data.ptr);

          spdlog::info("Receive from a channel");
          auto [complete, input] = worker.ipc_read().receive();

          if(complete) {
            _process_internal_message(worker, worker.ipc_read().message(), std::move(input));
          }
        }

        // spdlog::error(
        //     "{} {} ", events[i].events, static_cast<FunctionWorker*>(events[i].data.ptr)->pid()
        //);

        // FIXME: Received invocation request
        // add this to the work queue

        // FIXME: received completion from worker
        // mark as done and add

        // FIXME: received get-put
      }

      // walk over all functions in a queue, schedule whatever possible
      while (_workers.has_idle_workers()) {

        Invocation* invoc = _work_queue.next();

        if (!invoc) {
          break;
        }

        // schedule on an idle worker
        _workers.submit(*invoc);
      }
    }

    _workers.shutdown();
    // swap

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
