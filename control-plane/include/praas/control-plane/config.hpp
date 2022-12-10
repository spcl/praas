
#ifndef PRAAS__CONTROLL_PLANE_CONFIG_HPP
#define PRAAS__CONTROLL_PLANE_CONFIG_HPP

#include <praas/control-plane/backend.hpp>
#include <praas/control-plane/deployment.hpp>

#include <istream>
#include <optional>
#include <string>

#include <cereal/archives/json.hpp>

namespace cereal {
  struct JSONInputArchive;
} // namespace cereal

namespace praas::control_plane::config {

  struct HTTPServer {

    static constexpr int DEFAULT_THREADS_NUMBER = 1;
    static constexpr int DEFAULT_PORT = 8080;

    HTTPServer() { set_defaults(); }

    int port;
    int threads;
    int max_payload_size;
    bool enable_ssl;
    std::optional<std::string> ssl_server_cert;
    std::optional<std::string> ssl_server_key;

    void load(cereal::JSONInputArchive& archive);
    void set_defaults();
  };

  struct Workers {
    static constexpr int DEFAULT_THREADS_NUMBER = 1;

    int threads;

    void load(cereal::JSONInputArchive& archive);
    void set_defaults();
  };

  struct DownScaler {

    static constexpr int DEFAULT_POLLING_INTERVAL = 60;
    static constexpr int DEFAULT_SWAPPING_THRESHOLD = 360;

    int polling_interval;
    int swapping_threshold;

    void load(cereal::JSONInputArchive& archive);
    void set_defaults();
  };

  struct TCPServer {
    static constexpr int DEFAULT_PORT = 0;

    TCPServer() { set_defaults(); }

    int port;
    int io_threads;

    void load(cereal::JSONInputArchive& archive);
    void set_defaults();
  };

  struct Backend {};

  struct BackendLocal : Backend {};

  struct Deployment {};

  struct Config {

    HTTPServer http;
    Workers workers;
    DownScaler down_scaler;
    TCPServer tcpserver;

    deployment::Type deployment_type;

    backend::Type backend_type;
    //std::unique_ptr<Backend> backend;

    bool verbose;

    void set_defaults();

    void load(cereal::JSONInputArchive& archive);

    static Config deserialize(std::istream& in_stream);
  };

} // namespace praas::control_plane::config

#endif
