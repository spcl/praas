
#include "resources.hpp"

namespace praas::control_plane {

  void Resources::add_process(Process && p)
  {
    this->processes[p.process_id] = std::move(p);
  }

  Process* Resources::get_process(std::string process_id)
  {
    auto it = this->processes.find(process_id);
    return it != this->processes.end() ? &(*it).second : nullptr;
  }

  void Resources::add_session(Process & pr, std::string session_id)
  {
    Session* s = new Session{session_id};
    pr.sessions.push_back(s);
    this->sessions[pr.process_id + ";" + session_id] = s;
  }

  Session::Session(std::string session_id):
    session_id(session_id)
  {}

  Resources::~Resources()
  {
    for(auto & [k, v] : sessions)
      delete v;
  }
}
