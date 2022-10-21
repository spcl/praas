
#include <praas/control-plane/handle.hpp>

namespace praas::control_plane::process {

  void ProcessHandle::set_application(Application* application)
  {
    this->application = application;
  }

  void ProcessHandle::swap(state::SwapLocation& swap_loc)
  {
    // FIXME: send message to the function
  }

} // namespace praas::control_plane::process
