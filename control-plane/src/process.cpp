#include <praas/control-plane/process.hpp>

namespace praas::control_plane::process {

  std::string process::Process::name() const
  {
    return _name;
  }

  void process::Process::set_handle(backend::ProcessHandle&& handle)
  {
    auto lock = write_lock();
    _handle = std::move(handle);
    _status = Status::ALLOCATED;
  }

  const backend::ProcessHandle& process::Process::handle() const
  {
    return _handle.value();
  }

  bool process::Process::has_handle() const
  {
    return _handle.has_value();
  }

  process::Status process::Process::status() const
  {
    return _status;
  }

  void process::Process::set_status(Status status)
  {
    auto lock = write_lock();
    _status = status;
  }

  process::Process::read_lock_t process::Process::read_lock() const
  {
    return std::move(read_lock_t{_mutex});
  }

  process::Process::write_lock_t process::Process::write_lock() const
  {
    return write_lock_t{_mutex};
  }

} // namespace praas::control_plane::process
