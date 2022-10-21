#ifndef PRAAS_CONTROLL_PLANE_PROCESS_HPP
#define PRAAS_CONTROLL_PLANE_PROCESS_HPP

#include <praas/control-plane/backend.hpp>

#include <praas/control-plane/state.hpp>

#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace praas::control_plane {

  class Application;

} // namespace praas::control_plane

namespace praas::control_plane::process {

  enum class Status {

    ALLOCATING = 0,
    ALLOCATED,
    SWAPPED_OUT,
    SWAPPING_OUT,
    SWAPPING_IN,
    DELETED

  };

  struct DataPlaneMetrics {

    int32_t invocations;
    int32_t computation_time;
    std::chrono::time_point<std::chrono::system_clock> last_invocation;
    std::chrono::time_point<std::chrono::system_clock> last_report;

    DataPlaneMetrics() = default;
  };

  struct Resources {

    int32_t vcpus;
    int32_t memory;
    std::string sandbox_id;
  };

  class Process {
  public:
    friend class control_plane::Application;

    using lock_t = std::shared_mutex;
    using write_lock_t = std::unique_lock<lock_t>;
    using read_lock_t = std::shared_lock<lock_t>;

    Process(std::string name, ProcessHandle&& handle, Resources&& resources)
        : _status(Status::ALLOCATING), _handle(std::move(handle)), _name(std::move(name)),
          _resources(resources)
    {
    }

    ~Process() = default;

    Process(const Process& obj) = delete;
    Process& operator=(const Process& obj) = delete;

    Process(Process&& obj) noexcept
    {
      write_lock_t lock{obj._mutex};
      this->_name = obj._name;
      this->_status = obj._status;
      this->_handle = std::move(obj._handle);
      this->_status = std::move(obj._status);
      this->_resources = std::move(obj._resources);
    }

    Process& operator=(Process&& obj) noexcept
    {
      if (this != &obj) {
        write_lock_t lhs_lk(_mutex, std::defer_lock);
        write_lock_t rhs_lk(obj._mutex, std::defer_lock);
        std::lock(lhs_lk, rhs_lk);

        this->_name = obj._name;
        this->_handle = std::move(obj._handle);
        this->_status = obj._status;
        this->_status = obj._status;
        this->_resources = std::move(obj._resources);
      }
      return *this;
    }

    std::string name() const;

    const process::ProcessHandle& c_handle() const;

    process::ProcessHandle& handle();

    Status status() const;

    state::SessionState& state();

    read_lock_t read_lock() const;

    /**
     * @brief Acquires an exclusive write lock to the class.
     * This is only allowed to be called by the resource management explicitly.
     * This way we solve the issue of clients trying to "upgrade" to write lock
     * while having to always keep the read lock.
     *
     * @return instance of unique_lock
     */
    write_lock_t write_lock() const;

    void set_handle(process::ProcessHandle&& handle);
    void set_status(Status status);

  private:

    Status _status;

    std::optional<ProcessHandle> _handle;

    std::string _name;

    Resources _resources;

    // DataPlaneMetrics _metrics;

    state::SessionState _state;

    mutable lock_t _mutex;
  };

} // namespace praas::control_plane::process

#endif
