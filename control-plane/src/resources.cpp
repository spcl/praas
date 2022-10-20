
#include <praas/control-plane/resources.hpp>

#include <praas/common/exceptions.hpp>

namespace praas::control_plane {

  void Application::add_process(process::Process& process)
  {
    write_lock_t lock(_active_mutex);

    auto [it, succeed] = _active_processes.try_emplace(process.name(), process);

    if (!succeed) {
      throw praas::common::ObjectExists{process.name()};
    }
  }

  void Application::swap_process(std::string process_id)
  {
    throw common::NotImplementedError{};
  }

  void Application::delete_process(std::string process_id)
  {
    throw common::NotImplementedError{};
  }

  ///**
  // * @brief
  // *
  // * @param process_id [TODO:description]
  // */
  // void
  // update_metrics(std::string process_id, std::string auth_token, const
  // process::DataPlaneMetrics&);

  // void invoke(std::string fname, std::string process_id = "");

  std::string process::Process::name() const
  {
    return "";
  }

  void Resources::add_application(Application&& application)
  {

    if(application.name().length() == 0) {
      throw std::invalid_argument{"Application name cannot be empty"};
    }

    ConcurrentTable<Application>::rw_acc_t acc;
    bool inserted = _applications.insert(acc, application.name());
    if (inserted) {
      acc->second = std::move(application);
    } else {
      throw praas::common::ObjectExists{application.name()};
    }
  }

  void Resources::get_application(std::string application_name, Resources::ROAccessor& acc)
  {
    _applications.find(acc._accessor, application_name);
  }

  void Resources::delete_application(std::string application_name)
  {
    if(application_name.length() == 0) {
      throw std::invalid_argument{"Application name cannot be empty"};
    }

    ConcurrentTable<Application>::rw_acc_t acc;
    _applications.find(acc, application_name);
    if (acc.empty()) {
      throw praas::common::ObjectDoesNotExist{application_name};
    }
    // FIXME: schedule deletion of processes
    _applications.erase(acc);
  }

  std::string Application::name() const
  {
    return this->_name;
  }

  const Application* Resources::ROAccessor::get() const
  {
    return empty() ? nullptr : &_accessor->second;
  }

  bool Resources::ROAccessor::empty() const
  {
    return _accessor.empty();
  }

  // Process& Resources::add_process(Process&& p)
  //{
  //   return this->processes[p.process_id] = std::move(p);
  // }

  // Process* Resources::get_process(std::string process_id)
  //{
  //   auto it = this->processes.find(process_id);
  //   return it != this->processes.end() ? &(*it).second : nullptr;
  // }

  // Process* Resources::get_free_process(std::string)
  //{
  //   auto it = std::find_if(processes.begin(), processes.end(), [](auto& p) {
  //     return p.second.allocated_sessions < p.second.max_sessions;
  //   });
  //   if (it == this->processes.end())
  //     return nullptr;

  //  (*it).second.allocated_sessions++;
  //  return &(*it).second;
  //}

  // Session& Resources::add_session(Process& pr, std::string session_id)
  //{
  //   auto [it, success] = this->sessions.emplace(session_id, session_id);
  //   ;
  //   pr.sessions.push_back(&it->second);
  //   return it->second;
  // }

  // Session* Resources::get_session(std::string session_id)
  //{
  //   auto it = this->sessions.find(session_id);
  //   return it != this->sessions.end() ? &(*it).second : nullptr;
  // }

  // void Resources::remove_process(std::string)
  //{
  //   // FIXME: implement - remove process from epoll, deallocate resources
  //   throw std::runtime_error("not implemented");
  // }

  // void Resources::remove_session(Process& process, std::string session_id)
  //{
  //   // First clean the process
  //   auto it = std::find_if(
  //       process.sessions.begin(), process.sessions.end(),
  //       [session_id](auto& obj) { return obj->session_id == session_id; }
  //   );
  //   if (it == process.sessions.end())
  //     return;

  //  process.allocated_sessions--;
  //  process.sessions.erase(it);
  //}

  // Session::Session(std::string session_id)
  //     : session_id(session_id), allocated(false)
  //{
  // }

  // Resources::~Resources() {}
} // namespace praas::control_plane
