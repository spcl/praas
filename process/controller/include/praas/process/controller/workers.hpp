#ifndef PRAAS_PROCESS_CONTROLLER_WORKERS_HPP
#define PRAAS_PROCESS_CONTROLLER_WORKERS_HPP

#include <praas/process/controller/config.hpp>
#include <praas/process/controller/remote.hpp>
#include <praas/process/runtime/internal/buffer.hpp>
#include <praas/process/runtime/internal/functions.hpp>
#include <praas/process/runtime/internal/ipc/ipc.hpp>

#include <chrono>
#include <memory>
#include <optional>
#include <utility>

#include <cereal/external/rapidjson/rapidjson.h>

namespace praas::process {

  struct InvocationSource {

    static InvocationSource from_process(const std::string& remote_process)
    {
      return {remote::RemoteType::PROCESS, remote_process};
    }

    static InvocationSource from_local()
    {
      return {remote::RemoteType::LOCAL_FUNCTION, std::nullopt};
    }

    static InvocationSource from_dataplane()
    {
      return {remote::RemoteType::DATA_PLANE, std::nullopt};
    }

    static InvocationSource from_controlplane()
    {
      return {remote::RemoteType::CONTROL_PLANE, std::nullopt};
    }

    static InvocationSource from_source(remote::RemoteType source)
    {
      return {source, std::nullopt};
    }

    bool is_remote() const
    {
      return source == remote::RemoteType::DATA_PLANE || source == remote::RemoteType::PROCESS ||
             source == remote::RemoteType::CONTROL_PLANE;
    }

    bool is_local() const
    {
      return !is_remote();
    }

    remote::RemoteType source;
    std::optional<std::string> remote_process;
  };

  struct Invocation {

    Invocation(
        const std::string& fname, const std::string& invocation_key,
        const runtime::internal::Trigger* trigger, InvocationSource&& source
    )
        : trigger(trigger), source(source)
    {
      req.function_name(fname);
      req.invocation_id(invocation_key);
    }

    Invocation(const Invocation&) = delete;
    Invocation& operator=(const Invocation&) = delete;
    Invocation(Invocation&&) = default;
    Invocation& operator=(Invocation&&) = default;

    void confirm_payload()
    {
      req.buffers(payload.begin(), payload.end());
    }

    void start()
    {
      invocation_start = std::chrono::high_resolution_clock::now();
    }

    void end()
    {
      invocation_end = std::chrono::high_resolution_clock::now();
    }

    long duration() const
    {
      return std::chrono::duration_cast<std::chrono::microseconds>(
                 invocation_end - invocation_start
      )
          .count();
    }

    std::chrono::high_resolution_clock::time_point invocation_start, invocation_end;
    runtime::internal::ipc::InvocationRequest req;
    std::vector<runtime::internal::Buffer<char>> payload;
    const runtime::internal::Trigger* trigger;

    bool active{};

    InvocationSource source;
  };

  struct WorkQueue {

    WorkQueue(runtime::internal::Functions& functions) : _functions(functions) {}

    std::optional<std::string> add_payload(
        const std::string& fname, const std::string& key, runtime::internal::Buffer<char>&& buffer,
        InvocationSource&& source
    );

    Invocation* next();

    std::optional<Invocation> finish(const std::string& key);

    bool empty() const
    {
      return _pending_invocations.empty();
    }

    void lock()
    {
      _locked = true;
    }

  private:
    bool _locked = false;

    // FIFO but easy removal in the middle - we skip middle elements.
    std::vector<Invocation*> _pending_invocations;

    // All invocations - active, and pending.
    std::unordered_map<std::string, Invocation> _active_invocations;

    runtime::internal::Functions& _functions;
  };

  struct TriggerChecker : runtime::internal::TriggerVisitor {

    TriggerChecker(Invocation& invoc, WorkQueue& queue) : invocation(invoc), work_queue(queue) {}

    Invocation& invocation;
    WorkQueue& work_queue;
    bool ready{};

    void visit(const runtime::internal::DirectTrigger&) override;
  };

  struct FunctionWorker {

    FunctionWorker(
        const char** args, runtime::internal::ipc::IPCMode, std::string ipc_name, int ipc_msg_size,
        char** env = {}
    );

    runtime::internal::ipc::IPCChannel& ipc_write() const;

    runtime::internal::ipc::IPCChannel& ipc_read() const;

    int pid() const
    {
      return _pid;
    }

    bool busy() const
    {
      return _busy;
    }

    void busy(bool val)
    {
      _busy = val;
    }

  private:
    std::unique_ptr<runtime::internal::ipc::IPCChannel> _ipc_read;

    std::unique_ptr<runtime::internal::ipc::IPCChannel> _ipc_write;

    int _pid;

    bool _busy;
  };

  struct Workers {

    Workers(config::Controller& cfg);

    std::vector<FunctionWorker>& workers()
    {
      return _workers;
    }
    bool has_idle_workers() const;

    void submit(Invocation& invocation);

    void finish(FunctionWorker& worker);

    void shutdown();

    void shutdown_channels();

  private:
    void _launch_cpp(config::Controller& cfg, const std::string& ipc_name);

    void _launch_python(config::Controller& cfg, const std::string& ipc_name);

    FunctionWorker* _get_idle_worker();

    std::vector<FunctionWorker> _workers;

    int _worker_counter{};

    int _idle_workers{};

    std::shared_ptr<spdlog::logger> _logger;
  };

} // namespace praas::process

#endif
