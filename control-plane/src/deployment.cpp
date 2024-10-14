#include <praas/common/exceptions.hpp>
#include <praas/control-plane/config.hpp>
#include <praas/control-plane/deployment.hpp>

#include <memory>

namespace praas::control_plane::state {

  std::string_view DiskSwapLocation::root_path() const
  {
    return "/";
  }

  std::string DiskSwapLocation::path(std::string process_name) const
  {
    return fmt::format("local://{}", app_name);
  }

} // namespace praas::control_plane::state

namespace praas::control_plane::deployment {

  Type deserialize(std::string mode)
  {
    if (mode == "local") {
      return Type::LOCAL;
    } else if (mode == "aws") {
      return Type::AWS;
    } else if (mode == "redis") {
      return Type::REDIS;
    }
    throw common::PraaSException{"Unknown"};
  }

  std::unique_ptr<Deployment> Deployment::construct(const config::Config& cfg)
  {
    if (cfg.deployment_type == Type::LOCAL) {
      return std::make_unique<Local>();
    }
    spdlog::error("Unknown deployment type! {}", static_cast<int>(cfg.deployment_type));
    return nullptr;
  }

  std::unique_ptr<state::SwapLocation> Local::get_location(std::string app_name)
  {
    return std::make_unique<state::DiskSwapLocation>(app_name);
  }

  void Local::delete_swap(const state::SwapLocation&)
  {
    spdlog::error("Deleting swap is not supported for disk operations.");
  }

} // namespace praas::control_plane::deployment
