#include <praas/serving/docker/containers.hpp>

#include <praas/common/exceptions.hpp>

namespace praas::serving::docker {

  void Processes::add(const std::string& id, Process&& proc)
  {
    rw_acc_t acc;
    _processes.insert(acc, id);
    acc->second = std::move(proc);
  }

  void Processes::get(const std::string& id, ro_acc_t& acc) const
  {
    _processes.find(acc, id);
  }

  void Processes::get(const std::string& id, rw_acc_t& acc)
  {
    _processes.find(acc, id);
  }

  bool Processes::erase(const std::string& id, std::optional<std::string> container_id)
  {
    rw_acc_t acc;
    bool found = _processes.find(acc, id);

    if(found) {
      if(container_id.has_value() && acc->second.container_id == container_id.value()) {
        _processes.erase(acc);
        return true;
      }
    }
    return false;
  }

  void Processes::get_all(std::vector<Process>& processes)
  {
    for (auto& proc : _processes) {
      processes.push_back(proc.second);
    }
  }

} // namespace praas::serving::docker
