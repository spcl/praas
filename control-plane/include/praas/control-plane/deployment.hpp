
#ifndef PRAAS__CONTROLL_PLANE_DEPLOYMENT_HPP
#define PRAAS__CONTROLL_PLANE_DEPLOYMENT_HPP

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

#endif
