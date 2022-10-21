#ifndef PRAAS_CONTROLL_PLANE_HANDLE_HPP
#define PRAAS_CONTROLL_PLANE_HANDLE_HPP

#include <optional>
#include <string>

#include <sockpp/tcp_socket.h>

namespace praas::control_plane::backend {

  struct Backend;

} // namespace praas::control_plane::backend

namespace praas::control_plane {

  struct Application;

} // namespace praas::control_plane

namespace praas::control_plane::state {

  struct SwapLocation;

} // namespace praas::control_plane::state

namespace praas::control_plane::process {

  struct ProcessHandle {
    std::reference_wrapper<backend::Backend> backend;
    std::string instance_id;
    std::string resource_id;
    std::optional<sockpp::tcp_socket> connection;
    Application* application{};

    ProcessHandle(backend::Backend& backend, std::string instance_id, std::string resource_id)
        : backend(backend), instance_id(std::move(instance_id)),
          resource_id(std::move(resource_id)), connection(std::nullopt)
    {
    }

    ProcessHandle(const ProcessHandle&) = delete;
    ProcessHandle(ProcessHandle&&) = default;
    ProcessHandle& operator=(const ProcessHandle&) = delete;
    ProcessHandle& operator=(ProcessHandle&&) = default;
    ~ProcessHandle() = default;

    void set_application(Application*);
    void swap(state::SwapLocation& swap_loc);
  };

} // namespace praas::control_plane::process

#endif
