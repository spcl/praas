#include <praas/process/controller/workers.hpp>

#include <praas/common/exceptions.hpp>
#include <praas/common/util.hpp>
#include <praas/process/runtime/internal/buffer.hpp>
#include <praas/process/runtime/internal/functions.hpp>

#include <filesystem>
#include <fstream>
#include <optional>

#include <cereal/archives/json.hpp>
#include <spdlog/spdlog.h>

#include <sys/signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace praas::process {

  std::optional<std::string> WorkQueue::add_payload(
      const std::string& fname, const std::string& key, runtime::internal::Buffer<char>&& buffer,
      InvocationSource&& source
  )
  {
    auto it = _active_invocations.find(key);

    // Extend an existing pending invocation
    // FIXME: bug when we schedule two functions with the same key?
    if (it != _active_invocations.end() && !(*it).second.active) {
      it->second.payload.push_back(std::move(buffer));
    }
    // Create a new invocation
    else {

      const runtime::internal::Trigger* trigger = _functions.get_trigger(fname);
      if (!trigger) {
        std::string msg = fmt::format("Ignoring invocation of an unknown function {}", fname);
        spdlog::error(msg);
        return msg;
      }

      auto [it, inserted] = _active_invocations.emplace(
          std::piecewise_construct, std::forward_as_tuple(key),
          std::forward_as_tuple(fname, key, trigger, std::move(source))
      );

      if (!inserted) {
        std::string msg =
            fmt::format("Failed to insert a new invocation {} for function {}", key, fname);
        spdlog::error(msg);
        return msg;
      }
      SPDLOG_DEBUG("Inserted a new invocation {} for function {}", key, fname);

      it->second.payload.push_back(std::move(buffer));

      it->second.start();

      // Now add the function to the queue
      _pending_invocations.push_back(&it->second);
    }

    return std::nullopt;
  }

  Invocation* WorkQueue::next()
  {
    for (auto it = _pending_invocations.begin(); it != _pending_invocations.end(); ++it) {

      TriggerChecker visitor{*(*it), *this};

      // Check if the function is ready to be invoked
      (*it)->trigger->accept(visitor);

      if (visitor.ready) {
        Invocation* ptr = *it;
        _pending_invocations.erase(it);
        return ptr;
      }
    }
    // No function can be invoked now.
    return nullptr;
  }

  std::optional<Invocation> WorkQueue::finish(const std::string& key)
  {
    // Check if the function invocation exists and is not pending.
    auto it = _active_invocations.find(key);

    if (it == _active_invocations.end() || !(*it).second.active) {
      return std::nullopt;
    }
    it->second.end();

    Invocation invoc = std::move((*it).second);
    _active_invocations.erase(it);

    return invoc;
  }

  void TriggerChecker::visit(const runtime::internal::DirectTrigger&)
  {
    // Single argument, no dependencies - always ready
    ready = true;
  }

  FunctionWorker::FunctionWorker(
      const char** args, runtime::internal::ipc::IPCMode mode, std::string ipc_name,
      int ipc_msg_size, char** envp
  )
  {

    // IPC channels must be created before we fork - avoid race condition.
    if (mode == runtime::internal::ipc::IPCMode::POSIX_MQ) {
      _ipc_read = std::make_unique<runtime::internal::ipc::POSIXMQChannel>(
          ipc_name + "_read", runtime::internal::ipc::IPCDirection::READ, true, true, ipc_msg_size
      );
      _ipc_write = std::make_unique<runtime::internal::ipc::POSIXMQChannel>(
          ipc_name + "_write", runtime::internal::ipc::IPCDirection::WRITE, true, true, ipc_msg_size
      );
    }

    int mypid = fork();
    if (mypid < 0) {
      throw praas::common::PraaSException{
          fmt::format("Fork failed! {}, reason {} {}", mypid, errno, strerror(errno))};
    }

    if (mypid == 0) {

      mypid = getpid();
      auto out_file = ("invoker_" + std::to_string(mypid));

      spdlog::info("Invoker begins work on PID {}", mypid);
      int fd = open(out_file.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
      dup2(fd, 1);
      dup2(fd, 2);
      // int ret = execvp(args[0], const_cast<char**>(&args[0]));

      int ret = 0;
      if (envp) {
        ret = execvpe(args[0], const_cast<char**>(&args[0]), envp);
      } else {
        ret = execvp(args[0], const_cast<char**>(&args[0]));
      }
      if (ret == -1) {
        spdlog::error("Invoker process {} failed {}, reason {}", args[0], errno, strerror(errno));
        close(fd);
        exit(1);
      }

    } else {
      spdlog::info("Started invoker process with PID {}", mypid);
    }

    _pid = mypid;

    _busy = false;
  }

  runtime::internal::ipc::IPCChannel& FunctionWorker::ipc_read() const
  {
    return *_ipc_read;
  }

  runtime::internal::ipc::IPCChannel& FunctionWorker::ipc_write() const
  {
    return *_ipc_write;
  }

  void Workers::_launch_cpp(config::Controller& cfg, const std::string& ipc_name)
  {
    // Linux specific
    std::string exec_path = cfg.deployment_location.empty()
                                ? std::filesystem::canonical("/proc/self/exe").parent_path() /
                                      "invoker" / "cpp_invoker_exe"
                                : std::filesystem::path{cfg.deployment_location} / "bin" /
                                      "invoker" / "cpp_invoker_exe";
    const char* argv[] = {
        exec_path.c_str(),
        "--process-id",
        cfg.process_id.c_str(),
        "--ipc-mode",
        "posix_mq",
        "--ipc-name",
        ipc_name.c_str(),
        "--code-location",
        cfg.code.location.c_str(),
        "--code-config-location",
        cfg.code.config_location.c_str(),
        nullptr};

    _workers.emplace_back(argv, cfg.ipc_mode, ipc_name, cfg.ipc_message_size);
  }

  void Workers::_launch_python(config::Controller& cfg, const std::string& ipc_name)
  {
    std::string python_runtime =
        !cfg.code.language_runtime_path.empty() ? cfg.code.language_runtime_path : "python";

    std::filesystem::path deployment_path =
        cfg.deployment_location.empty()
            ? std::filesystem::canonical("/proc/self/exe").parent_path() / "invoker" / "python.py"
            : std::filesystem::path{cfg.deployment_location} / "bin" / "invoker" / "python.py";

    const char* argv[] = {
        python_runtime.c_str(),
        deployment_path.c_str(),
        "--process-id",
        cfg.process_id.c_str(),
        "--ipc-mode",
        "posix_mq",
        "--ipc-name",
        ipc_name.c_str(),
        "--code-location",
        cfg.code.location.c_str(),
        "--code-config-location",
        cfg.code.config_location.c_str(),
        nullptr};

    _workers.emplace_back(argv, cfg.ipc_mode, ipc_name, cfg.ipc_message_size);
  }

  Workers::Workers(config::Controller& cfg)
  {

    _logger = common::util::create_logger("Workers");

    common::util::assert_other(
        static_cast<int>(cfg.code.language), static_cast<int>(runtime::internal::Language::NONE)
    );

    for (int i = 0; i < cfg.function_workers; ++i) {

      std::string ipc_name;
      if (cfg.ipc_name_prefix.empty()) {
        ipc_name = fmt::format("/praas_queue_{}_{}", getpid(), _worker_counter++);
      } else {
        ipc_name =
            fmt::format("/{}_praas_queue_{}_{}", cfg.ipc_name_prefix, getpid(), _worker_counter++);
      }

      if (cfg.code.language == runtime::internal::Language::CPP) {
        _launch_cpp(cfg, ipc_name);
      } else if (cfg.code.language == runtime::internal::Language::PYTHON) {
        _launch_python(cfg, ipc_name);
      }
    }

    _idle_workers = cfg.function_workers;
  }

  FunctionWorker* Workers::_get_idle_worker()
  {
    if (_idle_workers == 0) {
      return nullptr;
    }

    for (FunctionWorker& worker : _workers) {
      if (!worker.busy()) {
        return &worker;
      }
    }
    // Should never happen
    return nullptr;
  }

  bool Workers::has_idle_workers() const
  {
    return _idle_workers > 0;
  }

  void Workers::submit(Invocation& invocation)
  {
    if (!has_idle_workers()) {
      throw praas::common::PraaSException{"No idle workers!"};
    }

    FunctionWorker* worker = _get_idle_worker();

    invocation.confirm_payload();

    SPDLOG_LOGGER_DEBUG(
        _logger, "Sending invocation of {}, with key {}", invocation.req.function_name(),
        invocation.req.invocation_id()
    );

    worker->ipc_write().send(invocation.req, invocation.payload);

    worker->busy(true);

    invocation.active = true;

    this->_idle_workers--;
  }

  void Workers::finish(FunctionWorker& worker)
  {
    // Mark the worker as inactive
    // Increase number of active worker

    worker.busy(false);
    _idle_workers++;
  }

  void Workers::shutdown_channels()
  {
    for (FunctionWorker& worker : _workers) {
      worker.ipc_read().shutdown();
      worker.ipc_write().shutdown();
    }
  }

  void Workers::shutdown()
  {
    for (FunctionWorker& worker : _workers) {
      kill(worker.pid(), SIGINT);
    }

    int status{};
    for (FunctionWorker& worker : _workers) {

      waitpid(worker.pid(), &status, 0);

      if (WIFEXITED(status)) {
        _logger->info("Worker child {} exited with status {}", worker.pid(), WEXITSTATUS(status));
      } else if (WIFSIGNALED(status)) {
        _logger->info("Worker child {} killed by signal {}", worker.pid(), WTERMSIG(status));
      }
    }
  }
}; // namespace praas::process
