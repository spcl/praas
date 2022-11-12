
#ifndef PRAAS__CONTROLL_PLANE_DEPLOYMENT_HPP
#define PRAAS__CONTROLL_PLANE_DEPLOYMENT_HPP

#include <praas/control-plane/state.hpp>

#if defined(WITH_AWS_DEPLOYMENT)
#include <praas/control-plane/aws.hpp>
#endif

#include <filesystem>

namespace praas::control_plane::state {

  struct DiskSwapLocation : public SwapLocation {
    std::filesystem::path fs_path;

    DiskSwapLocation(std::filesystem::path path) : SwapLocation(), fs_path(path) {}

    std::string_view root_path() const;
    std::string path(std::string process_name) const;
  };

#if defined(WITH_AWS_DEPLOYMENT)
  class AWSS3SwapLocation : SwapLocation {};
#endif

} // namespace praas::control_plane::state

namespace praas::control_plane::deployment {

  enum class Type {
    LOCAL,
#if defined(WITH_AWS_DEPLOYMENT)
    AWS,
#endif
  };

  class Deployment {
  public:
    virtual std::unique_ptr<state::SwapLocation> get_location(std::string process_name) = 0;
    virtual void delete_swap(const state::SwapLocation &) = 0;
  };

  class Local : public Deployment {
  public:
    Local(std::string fs_path) : _path(fs_path) {}

    std::unique_ptr<state::SwapLocation> get_location(std::string process_name) override;

    void delete_swap(const state::SwapLocation &) override;

  private:
    std::filesystem::path _path;
  };

} // namespace praas::control_plane::deployment

#endif
