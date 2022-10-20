#ifndef PRAAS_CONTROLL_PLANE_PROCESS_HPP
#define PRAAS_CONTROLL_PLANE_PROCESS_HPP

#include <praas/control-plane/backend.hpp>

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

    Process(std::string name, Resources&& resources)
        : _status(Status::ALLOCATING), _name(std::move(name)), _handle(std::nullopt),
          _resources(resources)
    {
    }

    Process(const Process& obj) = delete;
    Process& operator=(const Process& obj) = delete;

    Process(Process&& obj) noexcept
    {
      write_lock_t lock{obj._mutex};
      this->_name = obj._name;
      this->_status = obj._status;
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
        this->_status = obj._status;
        this->_status = std::move(obj._status);
        this->_resources = std::move(obj._resources);
      }
      return *this;
    }

    std::string name() const;

    const backend::ProcessHandle& handle() const;

    bool has_handle() const;

    Status status() const;

    read_lock_t read_lock() const;

  private:
    /**
     * @brief Acquires an exclusive write lock to the class.
     * This is only allowed to be called by the resource management explicitly.
     * This way we solve the issue of clients trying to "upgrade" to write lock
     * while having to always keep the read lock.
     *
     * @return instance of unique_lock
     */
    write_lock_t write_lock() const;

    void set_handle(backend::ProcessHandle&& handle);
    void set_status(Status status);

    Status _status;

    std::string _name;

    std::optional<backend::ProcessHandle> _handle;

    Resources _resources;

    // DataPlaneMetrics _metrics;

    // state::SessionState _state;

    mutable lock_t _mutex;
  };

} // namespace praas::control_plane::process

#endif
