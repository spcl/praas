
#include <stdexcept>

#include "resources.hpp"

namespace praas::control_plane {

  Process& Resources::add_process(Process && p)
  {
    return this->processes[p.process_id] = std::move(p);
  }

  Process* Resources::get_process(std::string process_id)
  {
    auto it = this->processes.find(process_id);
    return it != this->processes.end() ? &(*it).second : nullptr;
  }

  Process* Resources::get_free_process(std::string)
  {
    auto it = std::find_if(
      processes.begin(), processes.end(),
      [](auto & p) {
        return p.second.allocated_sessions < p.second.max_sessions;
      }
    );
    if(it == this->processes.end())
      return nullptr;

    (*it).second.allocated_sessions++;
    return &(*it).second;
  }


  Session& Resources::add_session(Process & pr, std::string session_id)
  {
    auto [it, success] = this->sessions.emplace(session_id, session_id);;
    pr.sessions.push_back(&it->second);
    return it->second;
  }

  Session* Resources::get_session(std::string session_id)
  {
    auto it = this->sessions.find(session_id);
    return it != this->sessions.end() ? &(*it).second : nullptr;
  }

  void Resources::remove_process(std::string)
  {
    // FIXME: implement - remove process from epoll, deallocate resources
    throw std::runtime_error("not implemented");
  }

  void Resources::remove_session(Process & process, std::string session_id)
  {
    // First clean the process
    auto it = std::find_if(
      process.sessions.begin(), process.sessions.end(),
      [session_id](auto & obj) {
        return obj->session_id == session_id;
      }
    );
    if(it == process.sessions.end())
      return;

    process.allocated_sessions--;
    process.sessions.erase(it);
  }

  Session::Session(std::string session_id):
    session_id(session_id),
    allocated(false)
  {}

  Resources::~Resources()
  {}
}
