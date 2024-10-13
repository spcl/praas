#include <praas/serving/docker/containers.hpp>

#include <praas/common/exceptions.hpp>

namespace praas::serving::docker {

  void Processes::add(const std::string& id, Process&& proc)
  {
    rw_acc_t acc;
    bool inserted = _processes.insert(acc, id);

    if (inserted) {
      acc->second = std::move(proc);
    } else {
      throw praas::common::ObjectExists{id};
    }
  }

  void Processes::get(const std::string& id, ro_acc_t& acc) const
  {
    _processes.find(acc, id);
  }

  void Processes::get(const std::string& id, rw_acc_t& acc)
  {
    _processes.find(acc, id);
  }

  bool Processes::erase(const std::string& id)
  {
    return _processes.erase(id);
  }

  void Processes::get_all(std::vector<Process>& processes)
  {
    for (auto& proc : _processes) {
      processes.push_back(proc.second);
    }
  }

} // namespace praas::serving::docker
