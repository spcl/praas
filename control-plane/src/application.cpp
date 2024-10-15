#include <praas/control-plane/application.hpp>

#include <praas/common/exceptions.hpp>
#include <praas/common/messages.hpp>
#include <praas/control-plane/backend.hpp>
#include <praas/control-plane/deployment.hpp>
#include <praas/control-plane/process.hpp>
#include <praas/control-plane/tcpserver.hpp>

#include <optional>
#include <spdlog/fmt/fmt.h>

namespace praas::control_plane {

  void Application::add_process(
      backend::Backend& backend, tcpserver::TCPServer& poller, const std::string& name,
      process::Resources&& resources, bool wait_for_allocation
  )
  {
    std::promise<std::string> p;
    this->add_process(
        backend, poller, name, std::move(resources),
        [&p](process::ProcessPtr process, const std::optional<std::string>& error_msg) {
          if (process) {
            p.set_value("");
          } else {
            p.set_value(error_msg.value());
          }
        },
        wait_for_allocation
    );
    auto msg = p.get_future().get();
    if (!msg.empty()) {
      throw common::FailedAllocationError{msg};
    }
  }

  void Application::add_process(
      backend::Backend& backend, tcpserver::TCPServer& poller, const std::string& name,
      process::Resources&& resources,
      std::function<void(process::ProcessPtr, const std::optional<std::string>&)>&& callback,
      bool wait_for_allocation
  )
  {

    if (name.length() <= 0) {
      throw praas::common::InvalidConfigurationError("Empty name");
    }

    double memory = std::stod(resources.memory);
    if (memory <= 0 || memory > backend.max_memory()) {
      throw praas::common::InvalidConfigurationError(
          fmt::format("Incorrect memory size {}", resources.memory)
      );
    }

    double vcpus = std::stod(resources.vcpus);
    if (vcpus <= 0 || vcpus > backend.max_vcpus()) {
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

    poller.add_process(process);
    process->set_creation_callback(std::move(callback), wait_for_allocation);

    backend.allocate_process(
        process, resources,
        [process, &poller, name, this](
          std::shared_ptr<backend::ProcessInstance>&& instance,
          const std::optional<std::string>& error
        ) {
          if (instance != nullptr) {
            process->set_handle(std::move(instance));

            // Avoid a race condition.
            // Created callback can be called by process connection, or by
            // the backend returning information.
            // We wait until both information are provided.
            process->created_callback(std::nullopt);

          } else {

            write_lock_t lock(_active_mutex);
            poller.remove_process(name);
            _active_processes.erase(name);
            process->created_callback(error);
          }
        }
    );
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

  void Application::get_controlplane_process(
      backend::Backend& backend, tcpserver::TCPServer& poller, process::Resources&& resources,
      std::function<void(process::ProcessPtr, const std::optional<std::string>& error)>&& callback
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
          callback(proc, std::nullopt);
          return;
        }
      }
    }

    // No process? create
    std::string name = fmt::format("controlplane-{}", _controlplane_processes.size());
    process::ProcessPtr process =
        std::make_shared<process::Process>(name, this, std::move(resources));
    process->set_creation_callback(std::move(callback), true);

    spdlog::info("Allocating process for invocation {}", name);
    poller.add_process(process);
    backend.allocate_process(
        process, resources,
        [=, this](
            std::shared_ptr<backend::ProcessInstance>&& instance,
            const std::optional<std::string>& msg
        ) {
          if (instance) {

            spdlog::info("Allocated process {}", name);
            process->set_handle(std::move(instance));

            // Avoid a race condition.
            // Created callback can be called by process connection, or by
            // the backend returning information.
            // We wait until both information are provided.
            process->created_callback(std::nullopt);

            {
              write_lock_t lock(_controlplane_mutex);
              _controlplane_processes.emplace_back(process);
            }
          } else {

            spdlog::error("Failed to allocate process!");
            process->created_callback(msg.value());
          }
        }
    );
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

  void Application::swap_process(
    std::string process_name,
    deployment::Deployment& deployment,
    std::function<void(size_t, double, const std::optional<std::string>&)>&& callback
  )
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
    proc.state().swap = deployment.get_location(_name);

    proc.swap(std::move(callback));

  }

  void Application::swapin_process(
    std::string process_name, backend::Backend& backend, tcpserver::TCPServer& poller,
    std::function<void(process::ProcessPtr, const std::optional<std::string>&)>&& callback
  )
  {
    read_lock_t application_lock(_swapped_mutex);

    // We cannot extract it immediately because we need to first lock the process
    auto iter = _swapped_processes.find(process_name);

    if (iter == _swapped_processes.end()) {
      throw praas::common::ObjectDoesNotExist{process_name};
    }

    // Lock the process first.
    process::Process& process = *(*iter).second;
    auto proc_lock = process.write_lock();
    process::ProcessPtr proc_ptr = (*iter).second;

    // Remove process from the container
    auto nh = _swapped_processes.extract(iter);
    application_lock.unlock();

    // Prepare swapping in
    process.set_status(process::Status::SWAPPING_IN);
    process.set_creation_callback(std::move(callback), true);
    poller.add_process((*iter).second);
    // Insert into swapped

    write_lock_t lock(_active_mutex);
    _active_processes.insert(std::move(nh));

    backend.allocate_process(
      proc_ptr, proc_ptr->_resources,
      [proc_ptr, &poller, process_name, this](
        std::shared_ptr<backend::ProcessInstance>&& instance,
        const std::optional<std::string>& error
      ) {
        if (instance != nullptr) {

          proc_ptr->set_handle(std::move(instance));

          // As in createrd process, we avoid a race condition.
          // Created callback can be called by process connection, or by
          // the backend returning information.
          // We wait until both information are provided.
          proc_ptr->created_callback(std::nullopt);

        } else {

          write_lock_t lock(_active_mutex);
          poller.remove_process(process_name);
          _active_processes.erase(process_name);
          proc_ptr->created_callback(error);
        }
      }
    );
  }

  void Application::swapped_process(std::string process_name, size_t size, double time)
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
      proc.swapped_callback(0, 0, "Cannot confirm a swap of non-swapping process");
      throw praas::common::InvalidProcessState("Cannot confirm a swap of non-swapping process");
    }

    // Remove process from the container
    auto nh = _active_processes.extract(iter);
    application_lock.unlock();

    proc.set_status(process::Status::SWAPPED_OUT);

    proc.close_connection();
    proc.set_handle(nullptr);

    proc.swapped_callback(size, time, std::nullopt);

    // Insert into swapped
    write_lock_t swapped_lock(_swapped_mutex);
    _swapped_processes.insert(std::move(nh));
  }

  void Application::closed_process(const process::ProcessPtr& ptr)
  {
    auto proc_lock = ptr->write_lock();

    if (ptr->status() != process::Status::SWAPPED_OUT && ptr->status() != process::Status::CLOSING) {

      spdlog::error("Failure! Closing not-swapped process {}, state {}", _name, ptr->status());

      ptr->set_status(process::Status::FAILURE);

      // Modify internal collections
      write_lock_t application_lock(_active_mutex);

      auto iter = _active_processes.find(ptr->name());
      if (iter != _active_processes.end()) {

        // If process failed during swapping, then notify client.
        (*iter).second->swapped_callback(0, 0, "Process closed unexpectedly during swapping");

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
      ptr->closed_connection();
    }
  }

  void Application::stop_process(
    std::string process_name, backend::Backend& backend,
    std::function<void(const std::optional<std::string>&)>&& callback
  )
  {
    if (process_name.length() == 0) {
      throw praas::common::InvalidConfigurationError("Process name cannot be empty");
    }

    write_lock_t application_lock(_active_mutex);

    auto iter = _active_processes.find(process_name);
    if (iter == _active_processes.end() || (*iter).second->status() != process::Status::ALLOCATED) {
      throw praas::common::InvalidConfigurationError("Process is not alive!");
    }

    (*iter).second->set_status(process::Status::CLOSING);
    backend.shutdown(
      (*iter).second->name(), (*iter).second,
      [this, process_name, callback=std::move(callback)](std::optional<std::string> msg) {

        if(!msg) {
          write_lock_t application_lock(_active_mutex);

          auto iter = _active_processes.find(process_name);
          if (iter != _active_processes.end()) {
            _active_processes.erase(iter);
          }
        }

        callback(msg);
      }
    );

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

  const ApplicationResources& Application::resources() const
  {
    return this->_resources;
  }

  int Application::get_process_count() const
  {
    return _active_processes.size() + _swapped_processes.size();
  }

  void Application::get_processes(std::vector<std::string>& results) const
  {
    read_lock_t application_lock(_active_mutex);

    for (const auto& [key, value] : _active_processes) {
      results.emplace_back(value->name());
    }
  }

  void Application::get_swapped_processes(std::vector<std::string>& results) const
  {
    read_lock_t application_lock(_swapped_mutex);

    for (const auto& [key, value] : _swapped_processes) {
      results.emplace_back(value->name());
    }
  }

} // namespace praas::control_plane
