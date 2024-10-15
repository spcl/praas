
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
    /// @brief Asynchronous version of creaing a new process within an application.
    ///
    /// @param[in] backend Cloud backend used to launch the process.
    /// @param[in] poller TCP server used to receive messages from this process.
    /// @param[in] name Process name.
    /// @param[in] resources Process resource specification.
    /// @param[in] callback Callback to be called on completion.
    ////////////////////////////////////////////////////////////////////////////////
    void add_process(
        backend::Backend& backend, tcpserver::TCPServer& poller, const std::string& name,
        process::Resources&& resources,
        std::function<void(process::ProcessPtr, const std::optional<std::string>&)>&& callback,
        bool wait_for_allocation = true
    );

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief Creates a new process within an application.
    ///
    /// @param[in] backend Cloud backend used to launch the process.
    /// @param[in] poller TCP server used to receive messages from this process.
    /// @param[in] name Process name.
    /// @param[in] resources Process resource specification.
    ////////////////////////////////////////////////////////////////////////////////
    void add_process(
        backend::Backend& backend, tcpserver::TCPServer& poller, const std::string& name,
        process::Resources&& resources, bool wait_for_allocation = true
    );

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief Acquires read access to a selected process.
    ///
    /// @param[in] name process name
    /// @return read lock instance and a pointer to process instance; nullptr if doesn't exist
    ////////////////////////////////////////////////////////////////////////////////
    std::tuple<process::Process::read_lock_t, process::Process*> get_process(const std::string& name
    ) const;

    void stop_process(
      std::string process_name, backend::Backend& backend,
      std::function<void(const std::optional<std::string>&)>&& callback
    );

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

    std::optional<std::tuple<process::Process::write_lock_t, process::Process*>>
    get_controlplane_process(const std::string& process_name);

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief Obtain a process that can be used for FaaS invocations.
    /// Finds the first process with a spare capacity. Otherwise, it allocates one.
    /// Result is passed to a callback due to asynchronous nature of process allocation.
    ///
    /// @param[in] backend cloud backend used for allocation
    /// @param[in] poller tcp server used by the process
    /// @param[in] resources
    /// @param[out] callback pass the acquired process instance; nullptr and error
    /// message if no process is available and no could be allocated
    ////////////////////////////////////////////////////////////////////////////////
    void get_controlplane_process(
        backend::Backend& backend, tcpserver::TCPServer& poller, process::Resources&& resources,
        std::function<void(process::ProcessPtr, const std::optional<std::string>& error)>&& callback
    );

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief TODO: implement?
    ///
    /// @param[[TODO:direction]] name [TODO:description]
    ////////////////////////////////////////////////////////////////////////////////
    void update_controlplane_process(const std::string& name);

    void swap_process(
      std::string process_name, deployment::Deployment& deployment,
      std::function<void(size_t, double, const std::optional<std::string>&)>&& callback
    );

    void swapin_process(
      std::string process_name, backend::Backend& backend, tcpserver::TCPServer& poller,
      std::function<void(process::ProcessPtr, const std::optional<std::string>&)>&& callback
    );

    void swapped_process(std::string process_name, size_t size, double time);

    void closed_process(const process::ProcessPtr& ptr);

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief Delete the swapped process.
    /// Throws an exception when the process does not exist or is still active.
    ///
    /// @param[in] process_name Process name.
    /// @param[in] deployment Deployment storing the swapped process state.
    ////////////////////////////////////////////////////////////////////////////////
    void delete_process(std::string process_name, deployment::Deployment& deployment);

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief Returns the list of active processes in an application
    ///
    /// @param[out] results list of process names
    ////////////////////////////////////////////////////////////////////////////////
    void get_processes(std::vector<std::string>& results) const;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief Returns the list of swapped processes in an application
    ///
    /// @param[out] results list of swapped process names
    ////////////////////////////////////////////////////////////////////////////////
    void get_swapped_processes(std::vector<std::string>& results) const;

    std::string name() const;

    const ApplicationResources& resources() const;

    int get_process_count() const;

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
    std::unordered_map<std::string, process::ProcessPtr> _controlplane_processes;
    int _controlplane_counter = 0;
  };

} // namespace praas::control_plane

#endif
