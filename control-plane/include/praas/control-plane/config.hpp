
#ifndef PRAAS__CONTROLL_PLANE_CONFIG_HPP
#define PRAAS__CONTROLL_PLANE_CONFIG_HPP

#include <praas/control-plane/backend.hpp>
#include <praas/control-plane/deployment.hpp>

#include <string>
#include <istream>

#include <cereal/archives/json.hpp>

namespace cereal {
  struct JSONInputArchive;
}

namespace praas::control_plane::config {

  struct HTTPServer {
    int port;
    int threads;
    bool enable_ssl;
    std::string ssl_server_cert;
    std::string ssl_server_key;
  };

  struct Workers {
    int threads;
  };

  struct DownScaler {
    int polling_interval;
    int scaling_threshold;
  };

  struct Backend {};

  struct BackendLocal : Backend {};

  struct Config {

    HTTPServer http;
    Workers workers;
    DownScaler down_scaler;

    deployment::Type deployment_type;

    backend::Type backend_type;
    std::unique_ptr<Backend> backend;

    bool verbose;

    void load(cereal::JSONInputArchive & archive);

    static Config deserialize(std::istream & in_stream);
  };

} // namespace praas::control_plane::config

#endif
