#ifndef PRAAS_CONTROLL_PLANE_STATE_HPP
#define PRAAS_CONTROLL_PLANE_STATE_HPP

#include <memory>
#include <string>

namespace praas::control_plane::state {

  struct SwapLocation {
    SwapLocation() = default;
    SwapLocation(const SwapLocation&) = default;
    SwapLocation(SwapLocation&&) = delete;
    SwapLocation& operator=(const SwapLocation&) = default;
    SwapLocation& operator=(SwapLocation&&) = delete;
    virtual ~SwapLocation() = default;
  };

  struct SessionState {

    int32_t size{};

    std::unique_ptr<SwapLocation> swap{};

    std::string session_id{};
  };

} // namespace praas::control_plane::state

#endif
