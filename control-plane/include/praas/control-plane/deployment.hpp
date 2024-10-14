
#ifndef PRAAS__CONTROLL_PLANE_DEPLOYMENT_HPP
#define PRAAS__CONTROLL_PLANE_DEPLOYMENT_HPP

#include <praas/control-plane/state.hpp>

#if defined(WITH_AWS_DEPLOYMENT)
#include <praas/control-plane/aws.hpp>
#endif

#include <filesystem>

namespace praas::control_plane::config {

  struct Config;

} // namespace praas::control_plane::config

namespace praas::control_plane::state {

  struct DiskSwapLocation : public SwapLocation {

    std::string app_name;

    DiskSwapLocation(std::string app_name):
      app_name(std::move(app_name))
    {}

    std::string_view root_path() const override;
    std::string path(std::string process_name) const override;
  };

#if defined(WITH_AWS_DEPLOYMENT)
  class AWSS3SwapLocation : SwapLocation {};
  class RedisSwapLocation : SwapLocation {};
#endif

} // namespace praas::control_plane::state

namespace praas::control_plane::deployment {

  enum class Type {
    LOCAL,
#if defined(WITH_AWS_DEPLOYMENT)
    AWS,
#endif
    REDIS
  };

  Type deserialize(std::string mode);

  class Deployment {
  public:
    virtual std::unique_ptr<state::SwapLocation> get_location(std::string app_name) = 0;
    virtual void delete_swap(const state::SwapLocation&) = 0;

    static std::unique_ptr<Deployment> construct(const config::Config& cfg);
  };

  class Local : public Deployment {
  public:
    Local() = default;

    // FIXME remove the fs path; we no longer need
    Local(std::string fs_path) : _path(std::move(fs_path)) {}

    std::unique_ptr<state::SwapLocation> get_location(std::string app_name) override;

    void delete_swap(const state::SwapLocation& /*unused*/) override;

  private:
    std::filesystem::path _path;
  };

} // namespace praas::control_plane::deployment

#endif
