#ifndef PRAAS_PROCESS_WORKERS_HPP
#define PRAAS_PROCESS_WORKERS_HPP

#include <praas/process/controller/config.hpp>
#include <praas/process/ipc/buffer.hpp>
#include <praas/process/ipc/ipc.hpp>

#include <memory>
#include <utility>

#include <cereal/external/rapidjson/rapidjson.h>

namespace praas::process {

  struct Trigger;

  struct Invocation {

    Invocation(std::string fname, std::string invocation_key, const Trigger* trigger):
      trigger(trigger)
    {
      req.function_name(fname);
      req.invocation_id(invocation_key);
    }

    void confirm_payload()
    {
      req.buffers(payload.begin(), payload.end());
    }

    ipc::InvocationRequest req;
    std::vector<ipc::Buffer<char>> payload;
    const Trigger* trigger;
  };

  struct WorkQueue {

    void initialize(std::istream & in_stream, config::Language language);

    void add_payload(std::string fname, std::string key, ipc::Buffer<char> && buffer);

    Invocation* next();

    void finish(std::string key);

    const Trigger* get_trigger(std::string name) const;

    bool empty() const
    {
      return _pending_invocations.empty();
    }

  private:

    void _parse_triggers(const rapidjson::Document & doc, std::string language);

    // FIFO but easy removal in the middle - we skip middle elements
    std::vector<Invocation*> _pending_invocations;

    std::unordered_map<std::string, Invocation> _active_invocations;

    std::unordered_map<std::string, std::unique_ptr<Trigger>> _functions;
  };

  struct Trigger {

    enum class Type {
      DIRECT,
      MULTI_SOURCE,
      BATCH,
      PIPELINE,
      DEPENDENCY
    };

    Trigger(std::string name):
      _name(std::move(name))
    {}

    ~Trigger() = default;

    virtual bool ready(Invocation&, WorkQueue&) const = 0;

    virtual Type type() const = 0;

    static std::unique_ptr<Trigger> parse(const rapidjson::Value & key, const rapidjson::Value & obj);

    std::string_view name() const;

  private:
    std::string _name;
  };

  struct DirectTrigger : Trigger {

    DirectTrigger(std::string name):
      Trigger(std::move(name))
    {}

    bool ready(Invocation& invocation, WorkQueue& queue) const override;

    Type type() const override;
  };

  struct FunctionWorker {

    FunctionWorker(const char ** args, ipc::IPCMode, std::string ipc_name, int ipc_msg_size);

    ipc::IPCChannel& ipc_write();

    ipc::IPCChannel& ipc_read();

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
    std::unique_ptr<ipc::IPCChannel> _ipc_read;

    std::unique_ptr<ipc::IPCChannel> _ipc_write;

    int _pid;

    bool _busy;
  };

  struct Workers {

    Workers(config::Controller & cfg);

    std::vector<FunctionWorker> & workers()
    {
      return _workers;
    }

    bool has_idle_workers() const;

    void submit(Invocation & invocation);

  private:

    FunctionWorker* _get_idle_worker();

    std::vector<FunctionWorker> _workers;

    int _worker_counter{};

    int _idle_workers{};
  };

}

#endif
