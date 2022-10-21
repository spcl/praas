#include <praas/control-plane/poller.hpp>

#include <praas/common/exceptions.hpp>

namespace praas::control_plane::poller {

  void Poller::add_handle(const process::ProcessHandle*)
  {
    throw common::NotImplementedError{};
  }

  void Poller::remove_handle(const process::ProcessHandle*)
  {
    throw common::NotImplementedError{};
  }

} // namespace praas::control_plane::poller
