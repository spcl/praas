#include <praas/control-plane/deployment.hpp>

namespace praas::control_plane::deployment {

  std::unique_ptr<state::SwapLocation> Local::get_location(std::string process_name)
  {
    return std::unique_ptr<state::DiskSwapLocation>(new state::DiskSwapLocation{
        (_path / "swaps" / process_name)});
  }

  void Local::delete_swap(const state::SwapLocation &)
  {
    // FIXME: warning log
    // We cannot remove swaps on other machines.
  }

} // namespace praas::control_plane::deployment
