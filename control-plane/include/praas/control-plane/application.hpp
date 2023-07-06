
#ifndef PRAAS_CONTROLL_PLANE_APPLICATION_HTTP
#define PRAAS_CONTROLL_PLANE_APPLICATION_HTTP

#include <praas/control-plane/process.hpp>

#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace praas::control_plane::backend {

  struct Backend;

} // namespace praas::control_plane::backend

namespace praas::control_plane::deployment {

  struct Deployment;

} // namespace praas::control_plane::deployment

namespace praas::control_plane::tcpserver {

  struct TCPServer;

} // namespace praas::control_plane::tcpserver

namespace praas::control_plane {

  class ApplicationResources {
  public:
    ApplicationResources() = default;

    ApplicationResources(std::string code_resource) : code_resource_name(std::move(code_resource))
    {
    }

    /**
     * @code_resource_name Name of container image or function that should be used as process.
     */
    std::string code_resource_name;
  };

  class Application {
  public:
    Application() = default;
    ~Application() = default;

    Application(std::string name, ApplicationResources&& resources)
        : _name(std::move(name)), _resources(std::move(resources))
    {
    }
    Application(const Application& obj) = delete;
    Application& operator=(const Application& obj) = delete;

    Application(Application&& obj) noexcept
    {
      write_lock_t lock{obj._active_mutex};
      write_lock_t swapped_lock{obj._swapped_mutex};
      std::lock(lock, swapped_lock);

      // Clang-tidy warns that these should be put in the initializer list.
      // However, we need to acquire locks first.
      // NOLINTBEGIN
      this->_name = obj._name;
      this->_resources = obj._resources;
      this->_active_processes = std::move(obj._active_processes);
      this->_swapped_processes = std::move(obj._swapped_processes);
      // NOLINTEND
    }

    Application& operator=(Application&& obj) noexcept
    {
      if (this != &obj) {
        write_lock_t active_lock(_active_mutex, std::defer_lock);
        write_lock_t swapped_lock(_swapped_mutex, std::defer_lock);
        write_lock_t other_active_lock(obj._active_mutex, std::defer_lock);
        write_lock_t other_swapped_lock(obj._swapped_mutex, std::defer_lock);
        std::lock(active_lock, swapped_lock, other_active_lock, other_swapped_lock);
        this->_name = obj._name;
        this->_resources = obj._resources;
        this->_active_processes = std::move(obj._active_processes);
        this->_swapped_processes = std::move(obj._swapped_processes);
      }
      return *this;
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief Creates a new process within an application.
    /// TODO this should take a callback and become a non-blocking operation
    ///
    /// @param[in] backend Cloud backend used to launch the process.
    /// @param[in] poller TCP server used to receive messages from this process.
    /// @param[in] name Process name.
    /// @param[in] resources Process resource specification.
    ////////////////////////////////////////////////////////////////////////////////
    void add_process(
        backend::Backend& backend, tcpserver::TCPServer& poller, const std::string& name,
        process::Resources&& resources
    );

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief Acquires read access to a selected process.
    ///
    /// @param[in] name process name
    /// @return read lock instance and a pointer to process instance; nullptr if doesn't exist
    ////////////////////////////////////////////////////////////////////////////////
    std::tuple<process::Process::read_lock_t, process::Process*> get_process(const std::string& name
    ) const;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief Acquires read access to a selected swapped process.
    ///
    /// @param[in] name process name
    /// @return read lock instance and a pointer to process instance; nullptr if doesn't exist
    ////////////////////////////////////////////////////////////////////////////////
    std::tuple<process::Process::read_lock_t, process::Process*>
    get_swapped_process(const std::string& name) const;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief Returns a process that can be used for FaaS invocations.
    /// Finds the first process with a spare capacity. Otherwise, it allocates one.
    ///
    /// @param[in] backend cloud backend used for allocation
    /// @param[in] poller
    /// @param[in] resources [TODO:description]
    /// @return read lock instance and a pointer to process instance; nullptr if no
    /// process is available and no could be allocated
    ////////////////////////////////////////////////////////////////////////////////
    std::tuple<process::Process::read_lock_t, process::Process*> get_controlplane_process(
        backend::Backend& backend, tcpserver::TCPServer& poller, process::Resources&& resources
    );

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief TODO: implement?
    ///
    /// @param[[TODO:direction]] name [TODO:description]
    ////////////////////////////////////////////////////////////////////////////////
    void update_controlplane_process(const std::string& name);

    void swap_process(std::string process_name, deployment::Deployment& deployment);

    void swapped_process(std::string process_name);

    void closed_process(const process::ProcessPtr& ptr);

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief Delete the swapped process.
    /// Throws an exception when the process does not exist or is still active.
    ///
    /// @param[in] process_name Process name.
    /// @param[in] deployment Deployment storing the swapped process state.
    ////////////////////////////////////////////////////////////////////////////////
    void delete_process(std::string process_name, deployment::Deployment& deployment);

    std::string name() const;

    const ApplicationResources& resources() const;

  private:
    using lock_t = std::shared_mutex;
    using write_lock_t = std::unique_lock<lock_t>;
    using read_lock_t = std::shared_lock<lock_t>;

    std::string _name;
    ApplicationResources _resources;

    // We need to be able to iterate across all processes.
    // Thus, we apply a read lock over the collection instead of using a concurrent map.
    mutable lock_t _active_mutex;
    std::unordered_map<std::string, process::ProcessPtr> _active_processes;

    mutable lock_t _swapped_mutex;
    std::unordered_map<std::string, process::ProcessPtr> _swapped_processes;

    lock_t _controlplane_mutex;
    std::vector<process::ProcessPtr> _controlplane_processes;
  };

} // namespace praas::control_plane

#endif
