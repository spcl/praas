
#ifndef PRAAS_CONTROLL_PLANE_BACKEND_HPP
#define PRAAS_CONTROLL_PLANE_BACKEND_HPP

#include <praas/control-plane/handle.hpp>

#include <memory>
#include <optional>
#include <string>

#include <sockpp/socket.h>

namespace praas::control_plane::config {

  struct Config;

} // namespace praas::control_plane::config

namespace praas::control_plane::process {

  struct Resources;

} // namespace praas::control_plane::process

namespace praas::control_plane::backend {

  enum class Type { LOCAL = 0 };

  struct Backend {

    Backend() = default;
    Backend(const Backend&) = default;
    Backend(Backend&&) = delete;
    Backend& operator=(const Backend&) = default;
    Backend& operator=(Backend&&) = delete;
    virtual ~Backend() = default;

    virtual void allocate_process(process::ProcessHandle& handle, const process::Resources& resources) = 0;

    // close process
    // swap process
    // invoke

    virtual int max_memory() const = 0;
    virtual int max_vcpus() const = 0;

    static std::unique_ptr<Backend> construct(const config::Config&);
  };

  // struct Backend {
  //   std::string controller_ip_address;
  //   int32_t controller_port;

  //  virtual ~Backend() {}

  //  virtual void allocate_process(
  //      std::string process_name, std::string process_id, int16_t max_sessions
  //  ) = 0;
  //  static Backend* construct(Options&);
  //};

  // struct LocalBackend : Backend {
  //   sockpp::tcp_connector connection;
  //   praas::common::ProcessRequest req;

  //  using Backend::controller_ip_address;
  //  using Backend::controller_port;

  //  LocalBackend(sockpp::tcp_connector&&);
  //  void allocate_process(
  //      std::string process_name, std::string process_id, int16_t max_sessions
  //  ) override;
  //  static LocalBackend* create(std::string local_server_addr);
  //};

  // struct AWSBackend : Backend {
  //   void allocate_process(
  //       std::string process_name, std::string process_id, int16_t max_sessions
  //   ) override;
  // };

} // namespace praas::control_plane::backend

#endif
