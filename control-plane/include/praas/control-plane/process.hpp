#ifndef PRAAS_CONTROLL_PLANE_PROCESS_HPP
#define PRAAS_CONTROLL_PLANE_PROCESS_HPP

#include <praas/control-plane/state.hpp>

#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <sockpp/tcp_socket.h>
#include <string>
#include <unordered_map>

#include <trantor/net/TcpConnection.h>
#include <uuid.h>

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
    CLOSED,
    FAILURE

  };

  struct DataPlaneMetrics {

    int32_t invocations{};
    int32_t computation_time{};
    uint64_t last_invocation{};
    std::chrono::time_point<std::chrono::system_clock> last_report;

    DataPlaneMetrics() = default;
  };

  struct DataPlaneConnection {
    using lock_t = std::shared_mutex;
    using write_lock_t = std::unique_lock<lock_t>;
    using read_lock_t = std::shared_lock<lock_t>;

    std::optional<sockpp::tcp_socket> connection{};

    read_lock_t read_lock() const;
    write_lock_t write_lock() const;

    mutable lock_t _mutex;
  };

  struct Resources {

    int32_t vcpus{};
    int32_t memory{};
    std::string sandbox_id;
  };

  struct Invocation {
    // using callback_t = std::function<void (const HttpResponsePtr &)>;
    using callback_t = std::function<void(const int&)>;
    callback_t callback;
    uuids::uuid invocation_id;
  };

  struct Handle {
    std::optional<std::string> instance_id{};
    std::optional<std::string> resource_id{};
  };

  class Process : public std::enable_shared_from_this<Process> {
  public:
    friend class praas::control_plane::Application;

    using lock_t = std::shared_mutex;
    using write_lock_t = std::unique_lock<lock_t>;
    using read_lock_t = std::shared_lock<lock_t>;

    Process(std::string name, Application* application, Resources&& resources)
        : _name(std::move(name)), _resources(resources), _application(application)
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

    const std::string& name() const;

    Status status() const;

    state::SessionState& state();

    Handle& handle();

    const Handle& c_handle() const;

    Application& application() const;

    void update_metrics(int32_t time, int32_t invocations, uint64_t timestamp);

    DataPlaneMetrics get_metrics() const;

    void connect(const trantor::TcpConnectionPtr& connectionPtr);

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

    // Modify the map of invocations.
    //void invoke(Invocation);

    void swap();

    void finish_invocation(Invocation);

    void set_status(Status status);

    void close_connection();

  private:
    std::string _name;

    Status _status{};

    Resources _resources;

    DataPlaneMetrics _metrics;

    trantor::TcpConnectionPtr _connection;

    state::SessionState _state;

    // Application reference does not change throughout process lifetime.
    //
    Application* _application;

    Handle _handle;

    mutable lock_t _mutex;

    mutable std::mutex _metrics_mutex;
  };

  using ProcessObserver = std::weak_ptr<Process>;
  using ProcessPtr = std::shared_ptr<Process>;

} // namespace praas::control_plane::process

#endif
