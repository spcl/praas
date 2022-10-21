#include <praas/control-plane/process.hpp>

namespace praas::control_plane::process {

  std::string Process::name() const
  {
    return _name;
  }

  void Process::set_handle(ProcessHandle&& handle)
  {
    _handle = std::move(handle);
    _status = Status::ALLOCATED;
  }

  const ProcessHandle& process::Process::handle() const
  {
    return _handle.value();
  }

  bool Process::has_handle() const
  {
    return _handle.has_value();
  }

  Status Process::status() const
  {
    return _status;
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

} // namespace praas::control_plane::process
