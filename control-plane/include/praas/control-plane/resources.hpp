
#ifndef PRAAS_CONTROLL_PLANE_RESOURCES_HPP
#define PRAAS_CONTROLL_PLANE_RESOURCES_HPP

#include <atomic>
#include <deque>
#include <future>
#include <memory>
#include <optional>
#include <random>
#include <unordered_map>
#include <utility>

#include <sockpp/tcp_socket.h>
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

  namespace state {

    struct SwapLocation {
      SwapLocation(const SwapLocation&) = default;
      SwapLocation(SwapLocation&&) = delete;
      SwapLocation& operator=(const SwapLocation&) = default;
      SwapLocation& operator=(SwapLocation&&) = delete;
      virtual ~SwapLocation() = default;

      virtual std::string location() const = 0;
    };

    class SessionState {
    public:
    private:
      int32_t _size;

      std::unique_ptr<SwapLocation> _swap;

      std::string _session_id;
    };

  } // namespace state

  namespace process {

    enum class Status {

      ACTIVE = 0,
      SWAPPED_OUT = 1,
      SWAPPING_OUT = 2,
      SWAPPING_IN = 3,
      DELETED = 4

    };

    struct DataPlaneMetrics {

      int32_t invocations;
      int32_t computation_time;
    };

    struct Resources {

      int32_t vcpus;
      int32_t memory;
    };

    class Process {
    public:
      std::string name() const;

    private:
      // FIXME: add reference to actual backend running the process
      //Resources _resources;

      //Status _status;

      //DataPlaneMetrics _metrics;

      //state::SessionState _state;
    };

  } // namespace process

  class Application {
  public:
    Application() = default;
    ~Application() = default;

    Application(std::string name) : _name(std::move(name)) {}
    Application(const Application & obj) = delete;
    Application& operator=(const Application & obj) = delete;

    Application(Application && obj) noexcept {
      write_lock_t lock{obj._active_mutex};
      this->_name = obj._name;
      this->_active_processes = std::move(obj._active_processes);
      this->_swapped_processes = std::move(obj._swapped_processes);
    }

    Application& operator=(Application && obj) noexcept
    {
      if (this != &obj)
      {
        write_lock_t lhs_lk(_active_mutex, std::defer_lock);
        write_lock_t rhs_lk(obj._active_mutex, std::defer_lock);
        std::lock(lhs_lk, rhs_lk);
        this->_name = obj._name;
        this->_active_processes = std::move(obj._active_processes);
        this->_swapped_processes = std::move(obj._swapped_processes);
      }
      return *this;
    }

    void add_process(process::Process&);

    void swap_process(std::string process_id);

    void delete_process(std::string process_id);

    /**
     * @brief
     *
     * @param process_id [TODO:description]
     */
    void
    update_metrics(std::string process_id, std::string auth_token, const process::DataPlaneMetrics&);


    void invoke(std::string fname, std::string process_id = "");

    std::string name() const;

  private:

    using lock_t = std::shared_mutex;
    using write_lock_t = std::unique_lock<lock_t>;
    using read_lock_t = std::shared_lock<lock_t>;

    std::string _name;

    lock_t _active_mutex;
    std::unordered_map<std::string, process::Process> _active_processes;

    lock_t _swapped_mutex;
    std::unordered_map<std::string, process::Process> _swapped_processes;
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

    /**
     * @brief Acquires a write-lock on the hash map to insert a new application.
     *
     * @param {name} desired application object.
     */
    void add_application(Application&& application);

    void get_application(std::string application_name, ROAccessor& acc);

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
