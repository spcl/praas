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

    virtual std::string_view root_path() const = 0;
    virtual std::string path(std::string process_name) const = 0;
  };

  struct SessionState {

    int32_t size{};

    std::unique_ptr<SwapLocation> swap{};

    std::string session_id{};
  };

} // namespace praas::control_plane::state

#endif
