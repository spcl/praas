#ifndef PRAAS_PROCESS_CONTROLLER_WORKERS_HPP
#define PRAAS_PROCESS_CONTROLLER_WORKERS_HPP

#include <praas/process/controller/config.hpp>
#include <praas/process/runtime/buffer.hpp>
#include <praas/process/runtime/functions.hpp>
#include <praas/process/runtime/ipc/ipc.hpp>

#include <memory>
#include <optional>
#include <utility>

#include <cereal/external/rapidjson/rapidjson.h>

namespace praas::process {

  struct InvocationSource {
    enum class Type { DATAPLANE = 0, LOCAL, PROCESS };

    static InvocationSource from_process(const std::string& remote_process)
    {
      return {Type::PROCESS, remote_process};
    }

    static InvocationSource from_local()
    {
      return {Type::LOCAL, std::nullopt};
    }

    static InvocationSource from_dataplane()
    {
      return {Type::DATAPLANE, std::nullopt};
    }

    bool is_remote() const
    {
      return source == Type::DATAPLANE || source == Type::PROCESS;
    }

    Type source;
    std::optional<std::string> remote_process;
  };

  struct Invocation {

    Invocation(
        const std::string& fname, const std::string& invocation_key,
        const runtime::functions::Trigger* trigger,
        InvocationSource && source
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

    runtime::ipc::InvocationRequest req;
    std::vector<runtime::Buffer<char>> payload;
    const runtime::functions::Trigger* trigger;

    bool active{};

    InvocationSource source;
  };

  struct WorkQueue {

    WorkQueue(runtime::functions::Functions& functions) : _functions(functions) {}

    void add_payload(const std::string& fname, const std::string& key, runtime::Buffer<char> && buffer, InvocationSource && source);

    Invocation* next();

    std::optional<Invocation> finish(const std::string& key);

    bool empty() const
    {
      return _pending_invocations.empty();
    }

  private:
    // FIFO but easy removal in the middle - we skip middle elements.
    std::vector<Invocation*> _pending_invocations;

    // All invocations - active, and pending.
    std::unordered_map<std::string, Invocation> _active_invocations;

    runtime::functions::Functions& _functions;
  };

  struct TriggerChecker : runtime::functions::TriggerVisitor {

    TriggerChecker(Invocation& invoc, WorkQueue& queue) : invocation(invoc), work_queue(queue) {}

    Invocation& invocation;
    WorkQueue& work_queue;
    bool ready{};

    void visit(const runtime::functions::DirectTrigger&) override;
  };

  struct FunctionWorker {

    FunctionWorker(
        const char** args, runtime::ipc::IPCMode, std::string ipc_name, int ipc_msg_size
    );

    runtime::ipc::IPCChannel& ipc_write();

    runtime::ipc::IPCChannel& ipc_read();

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
    std::unique_ptr<runtime::ipc::IPCChannel> _ipc_read;

    std::unique_ptr<runtime::ipc::IPCChannel> _ipc_write;

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

  private:
    FunctionWorker* _get_idle_worker();

    std::vector<FunctionWorker> _workers;

    int _worker_counter{};

    int _idle_workers{};
  };

} // namespace praas::process

#endif
