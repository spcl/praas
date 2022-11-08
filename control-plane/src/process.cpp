#include <praas/control-plane/process.hpp>
#include <praas/common/exceptions.hpp>
#include <spdlog/spdlog.h>

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

  void Process::connect(const trantor::TcpConnectionPtr &connectionPtr)
  {
    if(_status != Status::ALLOCATING){
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

} // namespace praas::control_plane::process
