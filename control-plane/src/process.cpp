#include <praas/common/exceptions.hpp>
#include <praas/common/messages.hpp>
#include <praas/control-plane/process.hpp>
#include <spdlog/spdlog.h>
#include <trantor/utils/MsgBuffer.h>

namespace praas::control_plane::process {

  DataPlaneConnection::read_lock_t DataPlaneConnection::read_lock() const
  {
    return read_lock_t{_mutex};
  }

  DataPlaneConnection::write_lock_t DataPlaneConnection::write_lock() const
  {
    return write_lock_t{_mutex};
  }

  const std::string& Process::name() const
  {
    return _name;
  }

  const Handle& Process::c_handle() const
  {
    return _handle;
  }

  Handle& Process::handle()
  {
    return _handle;
  }

  Application& Process::application() const
  {
    return *_application;
  }

  Status Process::status() const
  {
    return _status;
  }

  state::SessionState& Process::state()
  {
    return _state;
  }

  void Process::set_status(Status status)
  {
    _status = status;
  }

  Process::read_lock_t Process::read_lock() const
  {
    return read_lock_t{_mutex};
  }

  Process::write_lock_t Process::write_lock() const
  {
    return write_lock_t{_mutex};
  }

  void Process::connect(const trantor::TcpConnectionPtr& connectionPtr)
  {
    if (_status != Status::ALLOCATING) {
      throw praas::common::InvalidProcessState{"Can't register process"};
    }

    _connection = std::move(connectionPtr);
    _status = Status::ALLOCATED;
  }

  void Process::close_connection()
  {
    _status = Status::CLOSED;
    _connection.reset();
  }

  void Process::update_metrics(int32_t time, int32_t invocations, uint64_t timestamp)
  {
    std::unique_lock<std::mutex> lock{_metrics_mutex};

    auto cur_timestamp = std::chrono::system_clock::now();
    if (cur_timestamp >= _metrics.last_report) {
      _metrics.computation_time = time;
      _metrics.invocations = invocations;
      _metrics.last_invocation = timestamp;
      _metrics.last_report = cur_timestamp;
    }
  }

  DataPlaneMetrics Process::get_metrics() const
  {
    std::unique_lock<std::mutex> lock{_metrics_mutex};

    return _metrics;
  }

  void Process::swap()
  {
    if(!_connection) {
      return;
    }

    std::string_view swap_path = _state.swap->root_path();
    praas::common::message::SwapRequest msg;
    msg.path(swap_path);

    _connection->send(msg.bytes(), decltype(msg)::BUF_SIZE);
  }

} // namespace praas::control_plane::process
