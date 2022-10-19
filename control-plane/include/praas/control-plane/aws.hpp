
#ifndef PRAAS_CONTROLL_PLANE_AWS_HPP
#define PRAAS_CONTROLL_PLANE_AWS_HPP

#if defined(WITH_AWS_BACKEND)

namespace praas::control_plane::state {

  struct AWSS3SwapLocation {

    std::string location() const = 0;
  };

}; // namespace praas::control_plane::state

#endif

#endif
