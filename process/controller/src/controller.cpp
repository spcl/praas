#include <praas/process/controller/controller.hpp>

#include <praas/common/exceptions.hpp>
#include <praas/common/messages.hpp>
#include <praas/common/util.hpp>
#include <praas/process/controller/config.hpp>
#include <praas/process/controller/remote.hpp>
#include <praas/process/controller/swapper.hpp>
#include <praas/process/runtime/internal/buffer.hpp>
#include <praas/process/runtime/internal/ipc/messages.hpp>
#include <praas/process/runtime/internal/state.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>

#include <execinfo.h>
#include <ratio>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/time.h>

#include <spdlog/sinks/stdout_color_sinks.h>

namespace praas::process {

  Controller* INSTANCE = nullptr;

  void set_terminate(Controller* controller)
  {
    INSTANCE = controller;
    // Set a termination handler to ensure that IPC mechanisms are released when we quite.
    std::set_terminate([]() mutable {
      if (INSTANCE) {
        INSTANCE->shutdown();
      }
      abort();
    });
  }

  void signal_handler(int)
  {
    if (INSTANCE) {
      INSTANCE->shutdown();
    }
  }

  void failure_handler(int signum)
  {
    if (INSTANCE) {
      INSTANCE->shutdown_channels();
    }

    fprintf(stderr, "Unfortunately, the process has crashed - signal %d.\n", signum);
    void* array[10];
    size_t size;
    // get void*'s for all entries on the stack
    size = backtrace(array, 10);
    // print out all the frames to stderr
    fprintf(stderr, "Error: signal %d:\n", signum);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    exit(1);
  }

  void set_signals()
  {
    {
      // Catch SIGINT
      struct sigaction sigIntHandler;
      sigIntHandler.sa_handler = &signal_handler;
      sigemptyset(&sigIntHandler.sa_mask);
      sigIntHandler.sa_flags = 0;
      sigaction(SIGINT, &sigIntHandler, NULL);
    }

    {
      // Catch falure signals
      struct sigaction sa;
      memset(&sa, 0, sizeof(struct sigaction));
      sigemptyset(&sa.sa_mask);
      sa.sa_handler = failure_handler;
      sa.sa_flags = SA_SIGINFO;

      sigaction(SIGSEGV, &sa, NULL);
      sigaction(SIGTERM, &sa, NULL);
      sigaction(SIGPIPE, &sa, NULL);
      sigaction(SIGABRT, &sa, NULL);
    }
  }

  namespace {

    // This assumes that the pointer to the socket does NOT change after
    // submitting to epoll.
    template <typename T>
    bool epoll_apply(int epoll_fd, int fd, T* data, uint32_t epoll_events, int flags)
    {
      SPDLOG_DEBUG(
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
        _work_queue(_functions), _process_id(cfg.process_id)
  {

    auto sink = std::make_shared<spdlog::sinks::stderr_color_sink_st>();
    _logger = std::make_shared<spdlog::logger>("Controller", sink);
    _logger->set_pattern("[%H:%M:%S:%f] [%n] [P %P] [T %t] [%l] %v ");

    auto path = std::filesystem::path{cfg.code.location} / cfg.code.config_location;
    SPDLOG_LOGGER_DEBUG(_logger, "Loading function configuration from {}", path.c_str());
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


    // FIXME: do we want to make it optional?
    // Ensure that IPC mechanisms are released in case of an unhandled exception
    set_terminate(this);

    set_signals();
  }

  Controller::~Controller()
  {
    if (!_ending)
      shutdown();
    close(_event_fd);
    close(_epoll_fd);
  }

  void Controller::set_remote(remote::Server* server)
  {
    this->_server = server;
  }

  void Controller::remote_message(
      praas::common::message::MessageData&& msg, runtime::internal::Buffer<char>&& payload,
      std::string process_id
  )
  {
    {
      std::lock_guard<std::mutex> lock(_deque_lock);
      _external_queue.emplace_back(
          remote::RemoteType::PROCESS, process_id, msg, std::move(payload)
      );
    }
    uint64_t tmp = 1;
    common::util::assert_other(write(_event_fd, &tmp, sizeof(tmp)), -1);
  }

  void Controller::dataplane_message(
      praas::common::message::MessageData&& msg, runtime::internal::Buffer<char>&& payload
  )
  {
    {
      std::lock_guard<std::mutex> lock(_deque_lock);
      _external_queue.emplace_back(
          remote::RemoteType::DATA_PLANE, std::nullopt, msg, std::move(payload)
      );
    }
    uint64_t tmp = 1;
    common::util::assert_other(write(_event_fd, &tmp, sizeof(tmp)), -1);
  }

  void Controller::controlplane_message(
      praas::common::message::MessageData&& msg, runtime::internal::Buffer<char>&& payload
  )
  {
    {
      std::lock_guard<std::mutex> lock(_deque_lock);
      _external_queue.emplace_back(
          remote::RemoteType::CONTROL_PLANE, std::nullopt, msg, std::move(payload)
      );
    }
    uint64_t tmp = 1;
    common::util::assert_other(write(_event_fd, &tmp, sizeof(tmp)), -1);
  }

  void Controller::update_application(common::Application::Status status, std::string_view process)
  {
    {
      std::lock_guard<std::mutex> lock(_app_lock);
      _app_updates.emplace_back(status, std::string{process});
    }
    uint64_t tmp = 1;
    common::util::assert_other(write(_event_fd, &tmp, sizeof(tmp)), -1);
  }

  void
  Controller::_process_application_updates(const std::vector<common::ApplicationUpdate>& updates)
  {
    runtime::internal::ipc::ApplicationUpdate msg;

    for (const common::ApplicationUpdate& update : updates) {

      msg.process_id(update.process_id);
      msg.status_change(static_cast<int32_t>(update.status));

      for (FunctionWorker& worker : _workers.workers()) {
        worker.ipc_write().send(msg);
      }
    }

    for (const common::ApplicationUpdate& update : updates) {
      _application.update(update.status, update.process_id);
    }
  }

  void Controller::swap_out(std::string location)
  {
    spdlog::info("Request swapping out!");

    // Prevent new invocations
    _work_queue.lock();

    std::vector<std::tuple<std::string, message::Message>> msgs;
    _mailbox.all_state(msgs);

    std::unique_ptr<swapper::Swapper> swapper;
    if(location.starts_with("local://")) {

      auto loc = location.substr(std::string_view{"local://"}.size());
      loc = std::filesystem::path{loc} / this->process_id();

      swapper = std::make_unique<swapper::DiskSwapper>();

      auto begin = std::chrono::high_resolution_clock::now();
      auto size = swapper->swap_out(loc, msgs);
      auto end = std::chrono::high_resolution_clock::now();

      _server->swap_confirmation(
        size, std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() / 1000.0
      );

      this->shutdown();

    } else {
      spdlog::error("unimplemented");
      abort();
    }
  }

  void Controller::_process_external_message(ExternalMessage& msg)
  {
    auto parsed_msg = praas::common::message::MessageParser::parse(msg.msg);

    std::visit(
        runtime::internal::ipc::overloaded{
            [&, this](common::message::InvocationRequestPtr& req) mutable {
              SPDLOG_LOGGER_DEBUG(
                  _logger, "Received external invocation request of {}, key {}, inputs {}",
                  req.function_name(), req.invocation_id(), req.payload_size()
              );
              auto res = _work_queue.add_payload(
                  std::string{req.function_name()}, std::string{req.invocation_id()},
                  std::move(msg.payload),
                  (msg.source.has_value() ? InvocationSource::from_process(msg.source.value())
                                          : InvocationSource::from_source(msg.source_type))
              );

              if (res.has_value()) {
                _process_invocation_result(
                    (msg.source.has_value() ? InvocationSource::from_process(msg.source.value())
                                            : InvocationSource::from_source(msg.source_type)),
                    req.invocation_id(), -1,
                    runtime::internal::BufferAccessor<const char>{
                        res.value().data(), res.value().size()}
                );
              }
            },
            [&, this](common::message::InvocationResultPtr& req) mutable {
              // Is there are pending message for this message?
              std::vector<const FunctionWorker*> pending_workers;
              _pending_msgs.find_invocation(std::string{req.invocation_id()}, pending_workers);

              // FIXME: remove this double message type, our message types are broken
              runtime::internal::ipc::InvocationResult result;
              result.invocation_id(req.invocation_id());
              result.return_code(req.return_code());
              result.buffer_length(msg.payload.len);

              for (const FunctionWorker* worker : pending_workers) {

                SPDLOG_LOGGER_DEBUG(
                    _logger, "Sending external invocational result with key {}, message len {}",
                    result.invocation_id(), msg.payload.len
                );

                worker->ipc_write().send(result, msg.payload);
              }
            },
            [&, this](common::message::PutMessagePtr& req) mutable {
              // Is there are pending message for this message?
              const FunctionWorker* pending_worker =
                  _pending_msgs.find_get(std::string{req.name()}, std::string{req.process_id()});
              if (pending_worker) {

                SPDLOG_LOGGER_DEBUG(
                    _logger, "Replying message to {} with key {}, message len {}", _process_id,
                    req.name(), msg.payload.len
                );

                runtime::internal::ipc::GetRequest return_req;
                return_req.process_id(req.process_id());
                return_req.name(req.name());

                pending_worker->ipc_write().send(return_req, std::move(msg.payload));

              } else {

                int length = msg.payload.len;
                bool success = _mailbox.put(
                    std::string{req.name()}, std::string{req.process_id()}, msg.payload
                );
                if (!success) {
                  _logger->error("Could not store message to itself, with key {}", req.name());
                } else {
                  SPDLOG_LOGGER_DEBUG(
                      _logger, "Stored a message from {}, with key {}, length {}",
                      std::string{req.process_id()}, req.name(), length
                  );
                }
              }
            },
            [this](auto&) { _logger->error("Received unsupported message!"); }},
        parsed_msg
    );
  }

  void Controller::_process_internal_message(
      FunctionWorker& worker, const runtime::internal::ipc::Message& msg,
      runtime::internal::Buffer<char>&& payload
  )
  {
    auto parsed_msg = msg.parse();

    std::visit(
        runtime::internal::ipc::overloaded{
            [&, this](runtime::internal::ipc::InvocationResultParsed& req) mutable {
              _process_invocation_result(
                  worker, req.invocation_id(), req.return_code(), std::move(payload)
              );
            },
            [&, this](runtime::internal::ipc::InvocationRequestParsed& req) mutable {
              _process_invocation(worker, req, std::move(payload));
            },
            [&, this](runtime::internal::ipc::PutRequestParsed& req) mutable {
              _process_put(req, std::move(payload));
            },
            [&, this](runtime::internal::ipc::StateKeysRequestParsed& req) mutable {
              int length;

              // FIXME: buffer
              auto str = runtime::internal::StateKeys::serialize(_mailbox.state_keys());
              runtime::internal::ipc::StateKeysResult return_req;
              return_req.buffer_length(str.length());

              worker.ipc_write().send(
                  return_req,
                  runtime::internal::BufferAccessor<const char>(str.data(), str.length())
              );
            },
            [&, this](runtime::internal::ipc::GetRequestParsed& req) mutable {
              if (req.state()) {

                auto* buf = _mailbox.try_state(std::string{req.name()});
                if (buf) {

                  runtime::internal::ipc::GetRequest return_req;
                  return_req.name(req.name());
                  return_req.state(true);

                  // Send
                  worker.ipc_write().send(return_req, buf->accessor<const char>());

                } else {

                  runtime::internal::ipc::GetRequest return_req;
                  return_req.name(req.name());
                  return_req.state(true);

                  // Send
                  worker.ipc_write().send(return_req);
                }

              } else {
                auto buf = _mailbox.try_get(
                    std::string{req.name()},
                    req.process_id() == SELF_PROCESS ? _process_id : req.process_id()
                );
                if (buf.has_value()) {
                  runtime::internal::ipc::GetRequest return_req;
                  return_req.process_id(req.process_id());
                  return_req.name(req.name());

                  SPDLOG_LOGGER_DEBUG(
                      _logger, "Returned message for key {}, source {}, length {}", req.name(),
                      req.process_id(), buf.value().len
                  );
                  worker.ipc_write().send(return_req, std::move(buf.value()));
                } else {

                  _pending_msgs.insert_get(
                      std::string{req.name()},
                      req.process_id() == SELF_PROCESS ? _process_id : req.process_id(), worker
                  );
                  SPDLOG_LOGGER_DEBUG(
                      _logger, "Stored pending message for key {}, source {}", req.name(),
                      req.process_id()
                  );
                }
              }
            },
            [this](auto&) { _logger->error("Received unsupported message!"); }},
        parsed_msg
    );
  }

  void Controller::poll()
  {
    // FIXME: customize
    runtime::internal::BufferQueue<char> buffers(10, 1024);
    std::vector<ExternalMessage> msg;
    std::vector<common::ApplicationUpdate> updates;

    std::array<epoll_event, MAX_EPOLL_EVENTS> events;
    while (true) {

      int events_count = epoll_wait(_epoll_fd, events.data(), MAX_EPOLL_EVENTS, EPOLL_TIMEOUT);

      // Finish if we failed (but we were not interrupted), or when end was requested.
      if (_ending || (events_count == -1 && errno != EINVAL)) {
        break;
      }

      for (int i = 0; i < events_count; ++i) {

        // Wake-up signal
        if (events[i].data.ptr == this) {

          uint64_t read_val;
          [[maybe_unused]] int read_size = read(_event_fd, &read_val, sizeof(read_val));
          assert(read_size != -1);

          {
            std::unique_lock<std::mutex> lock(_deque_lock);

            size_t queue_size = _external_queue.size();
            if (queue_size > 0) {
              msg.resize(_external_queue.size());

              for (size_t j = 0; j < queue_size; ++j) {
                msg[j] = std::move(_external_queue.front());
                _external_queue.pop_front();
              }

              lock.unlock();

              for (size_t j = 0; j < msg.size(); ++j) {
                _process_external_message(msg[j]);
              }
              msg.clear();
            }
          }
          {
            std::unique_lock<std::mutex> lock(_app_lock);
            size_t queue_size = _app_updates.size();

            if (queue_size > 0) {
              updates.resize(queue_size);

              for (size_t j = 0; j < queue_size; ++j) {
                updates[j] = std::move(_app_updates.front());
                _app_updates.pop_front();
              }

              lock.unlock();

              _process_application_updates(updates);
              msg.clear();
            }
          }
        }
        // Message
        else {

          FunctionWorker& worker = *static_cast<FunctionWorker*>(events[i].data.ptr);

          auto [complete, input] = worker.ipc_read().receive();

          if (complete) {
            _process_internal_message(worker, worker.ipc_read().message(), std::move(input));
          }
        }
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

    _logger->info("Controller finished polling");
  }

  void Controller::start()
  {
    poll();
  }

  void Controller::shutdown_channels()
  {
    return _workers.shutdown_channels();
  }

  void Controller::shutdown()
  {
    _ending = true;

    _logger->info("Closing controller polling.");
  }

  void Controller::_process_invocation(
      FunctionWorker& worker, const runtime::internal::ipc::InvocationRequestParsed& req,
      runtime::internal::Buffer<char>&& payload
  )
  {
    SPDLOG_LOGGER_DEBUG(
        _logger, "Received internal invocation request of {}, status {}, input size {}",
        req.function_name(), req.invocation_id(), payload.len
    );

    // Avoid race condition. We need to store a pending invocation locally because processing
    // a result will depend on it.
    _pending_msgs.insert_invocation(req.invocation_id(), worker);
    if (req.process_id() == SELF_PROCESS || req.process_id() == _process_id) {

      auto res = _work_queue.add_payload(
          std::string{req.function_name()}, std::string{req.invocation_id()}, std::move(payload),
          InvocationSource::from_local()
      );
      if (res.has_value()) {
        _process_invocation_result(
            InvocationSource::from_local(), req.invocation_id(), -1,
            runtime::internal::BufferAccessor<const char>{res.value().data(), res.value().size()}
        );
      }

    } else {

      _server->invocation_request(
          req.process_id(), req.function_name(), req.invocation_id(), std::move(payload)
      );
    }
  }

  // Store the message data, and check if there is a pending invocation waiting for this result
  // FIXME: this should be a single type
  void Controller::_process_put(
      const runtime::internal::ipc::PutRequestParsed& req, runtime::internal::Buffer<char>&& payload
  )
  {
    SPDLOG_LOGGER_DEBUG(
        _logger, "Process put message with key {}, payload size {}", req.name(), payload.len
    );
    // local message or state message
    if (req.state()) {

      int length = payload.len;
      bool success = _mailbox.state(std::string{req.name()}, payload);
      if (!success) {
        _logger->error("Could not store state message to itself, with key {}", req.name());
      } else {
        SPDLOG_LOGGER_DEBUG(
            _logger, "Stored a state message to {}, with key {}, length {}", _process_id,
            req.name(), length
        );
      }

    } else if (req.process_id() == SELF_PROCESS || req.process_id() == _process_id) {

      // Is there are pending message for this message?
      const FunctionWorker* pending_worker =
          _pending_msgs.find_get(std::string{req.name()}, _process_id);
      if (pending_worker) {

        SPDLOG_LOGGER_DEBUG(
            _logger, "Replying message to {} with key {}, message len {}", _process_id, req.name(),
            payload.len
        );

        runtime::internal::ipc::GetRequest return_req;
        return_req.process_id(_process_id);
        return_req.name(req.name());

        pending_worker->ipc_write().send(return_req, std::move(payload));

      } else {

        int length = payload.len;
        bool success = _mailbox.put(std::string{req.name()}, _process_id, payload);
        if (!success) {
          _logger->error("Could not store message to itself, with key {}", req.name());
        } else {
          SPDLOG_LOGGER_DEBUG(
              _logger, "Stored a message to {}, with key {}, length {}", _process_id, req.name(),
              length
          );
        }
      }
    }
    // remote message
    else {
      _server->put_message(req.process_id(), req.name(), std::move(payload));
    }
  }

  void Controller::_process_invocation_result(
      const InvocationSource& source, std::string_view invocation_id, int return_code,
      runtime::internal::BufferAccessor<const char> payload
  )
  {
    // FIXME: send to the tcp server
    // FIXME: check work queue for the source
    if (source.is_remote()) {

      _server->invocation_result(
          source.source, source.remote_process, invocation_id, return_code, payload
      );

    } else {

      std::vector<const FunctionWorker*> pending_workers;
      _pending_msgs.find_invocation(invocation_id, pending_workers);

      // FIXME: remove this double message, our message types are broken
      runtime::internal::ipc::InvocationResult result;
      result.invocation_id(invocation_id);
      result.return_code(return_code);
      result.buffer_length(payload.len);

      for (const FunctionWorker* worker : pending_workers) {

        SPDLOG_LOGGER_DEBUG(
            _logger, "Replying invocation locally with key {}, message len {}",
            result.invocation_id(), payload.len
        );

        worker->ipc_write().send(result, payload);
      }
    }
  }

  void Controller::_process_invocation_result(
      FunctionWorker& worker, std::string_view invocation_id, int return_code,
      runtime::internal::BufferAccessor<const char> payload
  )
  {
    SPDLOG_LOGGER_DEBUG(
        _logger, "Received invocation result of {}, status {}, output size {}", invocation_id,
        return_code, payload.len
    );

    std::optional<Invocation> invoc = _work_queue.finish(std::string{invocation_id});
    if (invoc.has_value()) {
      Invocation& invocation = invoc.value();
      _logger->info("Finished invocation {}, it took {} us", invocation_id, invoc->duration());
      _process_invocation_result(invocation.source, invocation_id, return_code, payload);
    } else {
      _logger->error("Could not find invocation for ID {}", invocation_id);
    }
    _workers.finish(worker);
  }

} // namespace praas::process
