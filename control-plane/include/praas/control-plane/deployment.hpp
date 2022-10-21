
#ifndef PRAAS__CONTROLL_PLANE_DEPLOYMENT_HPP
#define PRAAS__CONTROLL_PLANE_DEPLOYMENT_HPP

#include <praas/control-plane/state.hpp>

#if defined(WITH_AWS_DEPLOYMENT)
#include <praas/control-plane/aws.hpp>
#endif

#if defined(WITH_AWS_DEPLOYMENT)

namespace praas::control_plane::state {

  class DiskSwapLocation : SwapLocation {};

  class AWSS3SwapLocation : SwapLocation {};

} // namespace praas::control_plane::state

#endif

namespace praas::control_plane::deployment {

  enum class Type {
    LOCAL,
#if defined(WITH_AWS_DEPLOYMENT)
    AWS,
#endif
  };

  class Deployment {
    virtual std::unique_ptr<state::SwapLocation> get_location(std::string process_name) = 0;
  };

  class Local : Deployment {
    virtual std::unique_ptr<state::SwapLocation> get_location(std::string process_name) {}
  };

} // namespace praas::control_plane::deployment

#endif
