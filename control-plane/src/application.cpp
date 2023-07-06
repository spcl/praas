#include <praas/control-plane/application.hpp>

#include <praas/common/exceptions.hpp>
#include <praas/common/messages.hpp>
#include <praas/control-plane/backend.hpp>
#include <praas/control-plane/deployment.hpp>
#include <praas/control-plane/process.hpp>
#include <praas/control-plane/tcpserver.hpp>

#include <spdlog/fmt/fmt.h>

namespace praas::control_plane {

  void Application::add_process(
      backend::Backend& backend, tcpserver::TCPServer& poller, const std::string& name,
      process::Resources&& resources
  )
  {

    if (name.length() <= 0) {
      throw praas::common::InvalidConfigurationError("Empty name");
    }

    if (resources.memory <= 0 || resources.memory > backend.max_memory()) {
      throw praas::common::InvalidConfigurationError(
          fmt::format("Incorrect memory size {}", resources.memory)
      );
    }

    if (resources.vcpus <= 0 || resources.vcpus > backend.max_vcpus()) {
      throw praas::common::InvalidConfigurationError(
          fmt::format("Incorrect number of vCPUs {}", resources.vcpus)
      );
    }

    // We lock the internal collection to write the new process.
    typename decltype(_active_processes)::iterator iter;
    bool succeed = false;
    process::ProcessPtr process;
    {
      write_lock_t lock(_active_mutex);

      process = std::make_shared<process::Process>(name, this, std::move(resources));

      std::tie(iter, succeed) = _active_processes.try_emplace(process->name(), process);

      if (!succeed) {
        throw praas::common::ObjectExists{process->name()};
      }
    }

    // Ensure that process is not modified.
    // process::Process& p = (*iter).second;
    // p.read_lock();

    try {

      poller.add_process(process);

      // FIXME: non-blocking, callback
      backend.allocate_process(process, resources);

    } catch (common::FailedAllocationError& err) {

      write_lock_t lock(_active_mutex);
      poller.remove_process(*process);
      _active_processes.erase(iter);
      throw err;
    }
  }

  std::tuple<process::Process::read_lock_t, process::Process*>
  Application::get_process(const std::string& name) const
  {
    read_lock_t lock(_active_mutex);

    auto iter = _active_processes.find(name);
    if (iter != _active_processes.end()) {
      return std::make_tuple(iter->second->read_lock(), iter->second.get());
    } else {
      throw praas::common::ObjectDoesNotExist{name};
    }
  }

  std::tuple<process::Process::read_lock_t, process::Process*>
  Application::get_controlplane_process(
      backend::Backend& backend, tcpserver::TCPServer& poller, process::Resources&& resources
  )
  {
    {
      read_lock_t lock(_controlplane_mutex);

      // FIXME: this needs to be a parameter - store it in the process
      int max_funcs_per_process = 1;

      for (auto& proc : _controlplane_processes) {

        int active_funcs = proc->active_invocations();
        if (active_funcs < max_funcs_per_process) {
          spdlog::info("Select existing process for invocation {}", proc->name());
          return std::make_tuple(proc->read_lock(), proc.get());
        }
      }
    }

    // No process? create
    std::string name = fmt::format("controlplane-{}", _controlplane_processes.size());
    process::ProcessPtr process =
        std::make_shared<process::Process>(name, this, std::move(resources));

    spdlog::info("Allocating process for invocation {}", name);
    poller.add_process(process);
    auto backend_allocate = backend.allocate_process(process, resources);
    if (!backend_allocate) {
      // FIXME: error handling
      // FIXME: remove from poller
      spdlog::error("Failed to allocate process!");
      abort();
    }

    process->set_handle(std::move(backend_allocate));
    spdlog::info("Allocated process {}", name);

    {
      write_lock_t lock(_controlplane_mutex);
      _controlplane_processes.emplace_back(process);
    }

    return std::make_tuple(process->read_lock(), process.get());
  }

  std::tuple<process::Process::read_lock_t, process::Process*>
  Application::get_swapped_process(const std::string& name) const
  {
    read_lock_t lock(_swapped_mutex);

    auto iter = _swapped_processes.find(name);
    if (iter != _swapped_processes.end()) {
      return std::make_tuple(iter->second->read_lock(), iter->second.get());
    } else {
      throw praas::common::ObjectDoesNotExist{name};
    }
  }

  void Application::swap_process(std::string process_name, deployment::Deployment& deployment)
  {
    if (process_name.length() == 0) {
      throw praas::common::InvalidConfigurationError("Application name cannot be empty");
    }

    read_lock_t application_lock(_active_mutex);

    // We cannot extract it immediately because we need to first lock the process
    auto iter = _active_processes.find(process_name);

    if (iter == _active_processes.end()) {
      throw praas::common::ObjectDoesNotExist{process_name};
    }

    process::Process& proc = *(*iter).second;

    // Exclusive access to the process
    // Keep lock to prevent others from modifying the process while we are modifying its state
    auto proc_lock = proc.write_lock();

    if (proc.status() != process::Status::ALLOCATED) {
      throw praas::common::InvalidProcessState("Cannot swap a non-allocated process");
    }

    // We no longer need to prevent modifications to the collection of processes.
    // Others will not be able to remove our process.
    application_lock.unlock();

    // No one else will now try to modify this process
    proc.set_status(process::Status::SWAPPING_OUT);

    // Swap the process
    proc.state().swap = deployment.get_location(process_name);

    proc.swap();
  }

  void Application::swapped_process(std::string process_name)
  {
    // Modify internal collections
    write_lock_t application_lock(_active_mutex);

    auto iter = _active_processes.find(process_name);
    if (iter == _active_processes.end()) {
      throw praas::common::ObjectDoesNotExist{process_name};
    }

    process::Process& proc = *(*iter).second;
    auto proc_lock = proc.write_lock();

    if (proc.status() != process::Status::SWAPPING_OUT) {
      throw praas::common::InvalidProcessState("Cannot confirm a swap of non-swapping process");
    }

    // Remove process from the container
    auto nh = _active_processes.extract(iter);
    application_lock.unlock();

    proc.set_status(process::Status::SWAPPED_OUT);

    // Insert into swapped
    write_lock_t swapped_lock(_swapped_mutex);
    _swapped_processes.insert(std::move(nh));
  }

  void Application::closed_process(const process::ProcessPtr& ptr)
  {
    auto proc_lock = ptr->write_lock();

    if (ptr->status() != process::Status::SWAPPED_OUT) {

      spdlog::error("Failure! Closing not-swapped process {}, state {}", _name, ptr->status());

      ptr->set_status(process::Status::FAILURE);

      // Modify internal collections
      write_lock_t application_lock(_active_mutex);

      auto iter = _active_processes.find(ptr->name());
      if (iter != _active_processes.end()) {
        _active_processes.erase(iter);
      } else {

        // FIXME: check for processes allocated by the control plane
        auto iter = _swapped_processes.find(ptr->name());
        if (iter != _swapped_processes.end()) {
          _swapped_processes.erase(iter);
        } else {
          spdlog::error("Unknown process {}", ptr->name());
        }
      }

    } else {
      ptr->close_connection();
    }
  }

  void Application::delete_process(std::string process_name, deployment::Deployment& deployment)
  {
    if (process_name.length() == 0) {
      throw praas::common::InvalidConfigurationError("Process name cannot be empty");
    }

    write_lock_t application_lock(_swapped_mutex);

    // We cannot close it immediately because we need to first lock the process
    auto iter = _swapped_processes.find(process_name);
    if (iter == _swapped_processes.end()) {
      throw praas::common::ObjectDoesNotExist{process_name};
    }

    process::Process& proc = *(*iter).second;

    deployment.delete_swap(*proc.state().swap);

    _swapped_processes.erase(iter);
  }

  std::string Application::name() const
  {
    return this->_name;
  }

} // namespace praas::control_plane