
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

  void Resources::add_session(Process &)
  {


  }
}
