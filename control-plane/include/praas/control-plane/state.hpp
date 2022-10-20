#ifndef PRAAS_CONTROLL_PLANE_STATE_HPP
#define PRAAS_CONTROLL_PLANE_STATE_HPP

#include <memory>
#include <string>

namespace praas::control_plane::state {

  struct SwapLocation {
    SwapLocation(const SwapLocation&) = default;
    SwapLocation(SwapLocation&&) = delete;
    SwapLocation& operator=(const SwapLocation&) = default;
    SwapLocation& operator=(SwapLocation&&) = delete;
    virtual ~SwapLocation() = default;

    virtual std::string location() const = 0;
  };

  class SessionState {
  public:
  private:
    int32_t _size;

    std::unique_ptr<SwapLocation> _swap;

    std::string _session_id;
  };

} // namespace praas::control_plane::state

#endif
