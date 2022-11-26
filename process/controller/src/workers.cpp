#include <praas/process/controller/workers.hpp>
#include <praas/common/exceptions.hpp>

#include <filesystem>
#include <fstream>

#include <cereal/archives/json.hpp>

namespace praas::process {

  void WorkQueue::initialize(std::istream & in_stream, config::Language language)
  {
    rapidjson::Document doc;
    rapidjson::IStreamWrapper wrapper{in_stream};
    doc.ParseStream(wrapper);

    _parse_triggers(doc, language_to_string(language));
  }

  void WorkQueue::_parse_triggers(const rapidjson::Document & doc, std::string language)
  {
    auto it = doc["functions"].FindMember(language.c_str());
    if(it == doc.MemberEnd() || !it->value.IsObject()) {
      throw common::InvalidJSON{fmt::format("Could not parse configuration for language {}", language)};
    }

    for(const auto& function_cfg : it->value.GetObject()) {

      auto trigger = Trigger::parse(function_cfg.name, function_cfg.value);
      _functions.insert(std::make_pair(function_cfg.name.GetString(), std::move(trigger)));

    }

  }

  const Trigger* WorkQueue::get_trigger(std::string name) const
  {
    auto it = _functions.find(name);
    if(it != _functions.end()) {
      return it->second.get();
    }
    return nullptr;
  }

  void WorkQueue::add_payload(std::string fname, std::string key, ipc::Buffer<char> && buffer)
  {
    auto it = _active_invocations.find(key);

    // Extend an existing invocation
    if(it != _active_invocations.end()) {
      it->second.payload.push_back(buffer);
    }
    // Create a new invocation
    else {

      const Trigger* trigger = get_trigger(fname);
      if(!trigger) {
        // FIXME: return error to the user
        spdlog::error("Ignoring invocation of an unknown function {}", fname);
      }

      auto [it, inserted] = _active_invocations.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(key),
        std::forward_as_tuple(fname, key, trigger)
      );

      if(!inserted) {
        // FIXME: return error to the user
        spdlog::error("Failed to insert a new invocation {} for function {}", key, fname);
      }

      it->second.payload.push_back(buffer);

      // Now add the function to the queue
      _pending_invocations.push_back(&it->second);
    }
  }

  Invocation* WorkQueue::next()
  {
    for(auto it = _pending_invocations.begin(); it != _pending_invocations.end(); ++it) {
      // Check if the function is ready to be invoked
      if((*it)->trigger->ready(*(*it), *this)) {
        _pending_invocations.erase(it);
        return (*it);
      }
    }
    // No function can be invoked now.
    return nullptr;
  }

  std::string_view Trigger::name() const
  {
    return _name;
  }

  std::unique_ptr<Trigger> Trigger::parse(const rapidjson::Value & key, const rapidjson::Value & obj)
  {
    std::string fname = key.GetString();

    auto it = obj.FindMember("trigger");
    if(it == obj.MemberEnd() || !it->value.IsObject()) {
      throw common::InvalidJSON{fmt::format("Could not parse trigger configuration for {}", fname)};
    }

    std::string trigger_type = it->value["type"].GetString();
    if(trigger_type == "direct") {
      return std::make_unique<DirectTrigger>(fname);
    }

    throw common::InvalidJSON{fmt::format("Could not parse trigger type {}", trigger_type)};
  }

  bool DirectTrigger::ready(Invocation&, WorkQueue&) const
  {
    // Single argument, no dependencies - always ready
    return true;
  }

  Trigger::Type DirectTrigger::type() const
  {
    return Type::DIRECT;
  }

  ipc::IPCChannel& FunctionWorker::ipc_read()
  {
    return *_ipc_read;
  }

  ipc::IPCChannel& FunctionWorker::ipc_write()
  {
    return *_ipc_write;
  }

  Workers::Workers(config::Controller & cfg)
  {
    for (int i = 0; i < cfg.function_workers; ++i) {

      std::string ipc_name = "/praas_queue_" + std::to_string(_worker_counter++);

      // FIXME: enable Python
      const char* argv[] = {"/work/serverless/2022/praas/code/build/process/bin/cpp_invoker_exe",
                            "--ipc-mode",
                            "posix_mq",
                            "--ipc-name",
                            ipc_name.c_str(),
                            nullptr};

      _workers.emplace_back(argv, cfg.ipc_mode, ipc_name, cfg.ipc_message_size);
    }

    _idle_workers = cfg.function_workers;
  }

  FunctionWorker* Workers::_get_idle_worker()
  {
    if(_idle_workers == 0) {
      return nullptr;
    }

    for(FunctionWorker& worker : _workers) {
      if(!worker.busy()) {
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

  void Workers::submit(Invocation & invocation)
  {
    if(!has_idle_workers()) {
      throw praas::common::PraaSException{"No idle workers!"};
    }

    FunctionWorker* worker = _get_idle_worker();

    invocation.confirm_payload();

    worker->ipc_write().send(invocation.req, invocation.payload);

    worker->busy(true);

    this->_idle_workers--;
  }

  void WorkQueue::finish(std::string key)
  {
    // Check if the function invocation exists
    // Mark the worker as inactive
    // Increase number of active workers
    // Handle the invocation response
  }

};
