
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

  Session::Session(std::string session_id):
    session_id(session_id),
    allocated(false)
  {}

  Resources::~Resources()
  {}
}
