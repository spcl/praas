
#ifndef PRAAS__CONTROLL_PLANE_DEPLOYMENT_HPP
#define PRAAS__CONTROLL_PLANE_DEPLOYMENT_HPP

#include <praas/control-plane/state.hpp>

#if defined(WITH_AWS_DEPLOYMENT)
#include <praas/control-plane/aws.hpp>
#endif

namespace praas::control_plane::deployment {

  enum class Type {
#if defined(WITH_AWS_DEPLOYMENT)
    AWS,
#endif
  };

} // namespace praas::control_plane::deployment

#if defined(WITH_AWS_DEPLOYMENT)

namespace praas::control_plane::state {

  class AWSS3SwapLocation : SwapLocation {};

} // namespace praas::control_plane::state

#endif

#endif
