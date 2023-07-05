#include <praas/control-plane/config.hpp>
#include <praas/control-plane/deployment.hpp>

#include <memory>

namespace praas::control_plane::state {

  std::string_view DiskSwapLocation::root_path() const
  {
    return fs_path.c_str();
  }

  std::string DiskSwapLocation::path(std::string process_name) const
  {
    return (fs_path / "swaps" / process_name).c_str();
  }

} // namespace praas::control_plane::state

namespace praas::control_plane::deployment {

  std::unique_ptr<Deployment> Deployment::construct(const config::Config& cfg)
  {
    if (cfg.deployment_type == Type::LOCAL) {
      return std::make_unique<Local>();
    }
    return nullptr;
  }

  std::unique_ptr<state::SwapLocation> Local::get_location(std::string process_name)
  {
    return std::make_unique<state::DiskSwapLocation>(_path);
  }

  void Local::delete_swap(const state::SwapLocation&)
  {
    // FIXME: warning log
    // We cannot remove swaps on other machines.
  }

} // namespace praas::control_plane::deployment
