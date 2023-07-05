
#ifndef PRAAS_CONTROLL_PLANE_RESOURCES_HPP
#define PRAAS_CONTROLL_PLANE_RESOURCES_HPP

#include <praas/control-plane/backend.hpp>
#include <praas/control-plane/deployment.hpp>
#include <praas/control-plane/process.hpp>
#include <praas/control-plane/tcpserver.hpp>

#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>

#include <tbb/concurrent_hash_map.h>

namespace praas::control_plane {

  template <typename Value, typename Key = std::string>
  struct ConcurrentTable {

    // IntelTBB concurrent hash map, with a default string key
    using table_t = oneapi::tbb::concurrent_hash_map<Key, Value>;

    // Equivalent to receiving a read-write lock. Should be used only for
    // modifying contents.
    using rw_acc_t = typename oneapi::tbb::concurrent_hash_map<Key, Value>::accessor;

    // Read lock. Guarantees that data is safe to access, as long as we keep the
    // accessor
    using ro_acc_t = typename oneapi::tbb::concurrent_hash_map<Key, Value>::const_accessor;
  };

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

  class Resources {
  public:
    class ROAccessor {
    public:
      const Application* get() const;

      bool empty() const;

    private:
      using ro_acc_t = ConcurrentTable<Application>::ro_acc_t;
      ro_acc_t _accessor;

      friend class Resources;
    };

    class RWAccessor {
    public:
      Application* get() const;

      bool empty() const;

    private:
      using rw_acc_t = ConcurrentTable<Application>::rw_acc_t;
      rw_acc_t _accessor;

      friend class Resources;
    };

    /**
     * @brief Acquires a write-lock on the hash map to insert a new application.
     *
     * @param {name} desired application object.
     */
    void add_application(Application&& application);

    void get_application(std::string application_name, ROAccessor& acc);
    void get_application(std::string application_name, RWAccessor& acc);

    /**
     * @brief Acquires a write-lock on the hash map to remove application, returning
     *
     * @param application_name
     */
    void delete_application(std::string application_name);

  private:
    ConcurrentTable<Application>::table_t _applications;
  };

  // struct PendingAllocation {
  //   std::string payload;
  //   std::string function_name;
  //   std::string function_id;
  // };

  // struct Resources {

  //  // Since we store elements in a C++ hash map, we have a gurantee that
  //  // pointers and references to elements are valid even after a rehashing.
  //  // Thus, we can safely store pointers in epoll structures, and we know that
  //  // after receiving a message from process, the pointer to Process data
  //  // structure will not change, even if we significantly altered the size of
  //  // this container.

  //  // session_id -> session
  //  std::unordered_map<std::string, Session> sessions;
  //  // process_id -> process resources
  //  std::unordered_map<std::string, Process> processes;
  //  // FIXME: processes per global process
  //  std::mutex _sessions_mutex;

  //} // namespace praas::control_plane
} // namespace praas::control_plane

#endif
