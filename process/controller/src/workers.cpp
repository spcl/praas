#include <praas/process/controller/workers.hpp>

#include <praas/common/exceptions.hpp>
#include <praas/common/util.hpp>
#include <praas/process/runtime/buffer.hpp>
#include <praas/process/runtime/functions.hpp>

#include <filesystem>
#include <fstream>

#include <cereal/archives/json.hpp>
#include <spdlog/spdlog.h>

#include <sys/signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace praas::process {

  void WorkQueue::add_payload(
      const std::string& fname, const std::string& key, runtime::Buffer<char>&& buffer,
      InvocationSource&& source
  )
  {
    auto it = _active_invocations.find(key);

    // Extend an existing invocation
    if (it != _active_invocations.end()) {
      it->second.payload.push_back(std::move(buffer));
    }
    // Create a new invocation
    else {

      const runtime::functions::Trigger* trigger = _functions.get_trigger(fname);
      if (!trigger) {
        // FIXME: return error to the user
        spdlog::error("Ignoring invocation of an unknown function {}", fname);
        return;
      }

      auto [it, inserted] = _active_invocations.emplace(
          std::piecewise_construct, std::forward_as_tuple(key),
          std::forward_as_tuple(fname, key, trigger, std::move(source))
      );

      if (!inserted) {
        // FIXME: return error to the user
        spdlog::error("Failed to insert a new invocation {} for function {}", key, fname);
      }

      it->second.payload.push_back(std::move(buffer));

      // Now add the function to the queue
      _pending_invocations.push_back(&it->second);
    }
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

    Invocation invoc = std::move((*it).second);
    _active_invocations.erase(it);

    return invoc;
  }

  void TriggerChecker::visit(const runtime::functions::DirectTrigger&)
  {
    // Single argument, no dependencies - always ready
    ready = true;
  }

  FunctionWorker::FunctionWorker(
      const char** args, runtime::ipc::IPCMode mode, std::string ipc_name, int ipc_msg_size,
      char** envp
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
      // int ret = execvp(args[0], const_cast<char**>(&args[0]));

      int ret = execvpe(args[0], const_cast<char**>(&args[0]), envp);
      if (ret == -1) {
        spdlog::error("Invoker process {} failed {}, reason {}", args[0], errno, strerror(errno));
        close(fd);
        exit(1);
      }

    } else {
      spdlog::info("Started invoker process with PID {}", mypid);
    }

    _pid = mypid;

    if (mode == runtime::ipc::IPCMode::POSIX_MQ) {
      _ipc_read = std::make_unique<runtime::ipc::POSIXMQChannel>(
          ipc_name + "_read", runtime::ipc::IPCDirection::READ, true, ipc_msg_size
      );
      _ipc_write = std::make_unique<runtime::ipc::POSIXMQChannel>(
          ipc_name + "_write", runtime::ipc::IPCDirection::WRITE, true, ipc_msg_size
      );
    }

    _busy = false;
  }

  runtime::ipc::IPCChannel& FunctionWorker::ipc_read() const
  {
    return *_ipc_read;
  }

  runtime::ipc::IPCChannel& FunctionWorker::ipc_write() const
  {
    return *_ipc_write;
  }

  void Workers::_launch_cpp(config::Controller& cfg, const std::string& ipc_name)
  {
    std::string exec_path = cfg.deployment_location.empty()
                                ? std::filesystem::path{"invoker"} / "cpp_invoker_exe"
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
            ? std::filesystem::path{"invoker"} / "python.py"
            : std::filesystem::path{cfg.deployment_location} / "bin" / "invoker" / "python.py";

    // From process/bin/invoker/python.py -> process
    auto python_lib_path = deployment_path.parent_path().parent_path().parent_path();
    std::string env_var = fmt::format("PYTHONPATH={}", python_lib_path.c_str());
    // This is safe because execvpe does not modify contents
    char* envp[] = {
      const_cast<char*>(env_var.c_str()),
      0
    };

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

    _workers.emplace_back(argv, cfg.ipc_mode, ipc_name, cfg.ipc_message_size, envp);
  }

  Workers::Workers(config::Controller& cfg)
  {

    common::util::assert_other(cfg.code.language, runtime::functions::Language::NONE);

    for (int i = 0; i < cfg.function_workers; ++i) {

      std::string ipc_name;
      if(cfg.ipc_name_prefix.empty()) {
         ipc_name = fmt::format("/praas_queue_{}_{}", getpid(), _worker_counter++);
      } else {
         ipc_name = fmt::format("/{}_praas_queue_{}_{}", cfg.ipc_name_prefix, getpid(), _worker_counter++);
      }

      if (cfg.code.language == runtime::functions::Language::CPP) {
        _launch_cpp(cfg, ipc_name);
      } else if (cfg.code.language == runtime::functions::Language::PYTHON) {
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

    spdlog::info(
        "Sending invocation of {}, with key {}", invocation.req.function_name(),
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

    int status;
    for (FunctionWorker& worker : _workers) {

      waitpid(worker.pid(), &status, 0);

      if (WIFEXITED(status)) {
        spdlog::info("Worker child {} exited with status {}", worker.pid(), WEXITSTATUS(status));
      } else if (WIFSIGNALED(status)) {
        spdlog::info("Worker child {} killed by signal {}", worker.pid(), WTERMSIG(status));
      }
    }
  }
}; // namespace praas::process
