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

#if defined(WITH_AWS_DEPLOYMENT)
  std::string_view AWSS3SwapLocation::root_path() const
  {
    return "/";
  }

  std::string AWSS3SwapLocation::path(std::string process_name) const
  {
    return fmt::format("s3://{}", app_name);
  }
#endif

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
    } else if (cfg.deployment_type == Type::AWS) {

      auto cfg_ptr = dynamic_cast<config::AWSDeployment*>(cfg.deployment.get());
      if(!cfg_ptr) {
        spdlog::error("Wrong deployment config type!");
        return nullptr;
      }

      return std::make_unique<AWS>(cfg_ptr->s3_bucket);
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

#if defined(WITH_AWS_DEPLOYMENT)
  std::unique_ptr<state::SwapLocation> AWS::get_location(std::string app_name)
  {
    return std::make_unique<state::AWSS3SwapLocation>(app_name);
  }

  void AWS::delete_swap(const state::SwapLocation&)
  {
    spdlog::error("Deleting swap is not yet supported!");
  }
#endif

} // namespace praas::control_plane::deployment
