#include <praas/control-plane/config.hpp>

#include <praas/common/exceptions.hpp>
#include <praas/common/util.hpp>
#include <praas/control-plane/backend.hpp>

#include <optional>
#include <stdexcept>

#include <cereal/archives/json.hpp>
#include <cereal/details/helpers.hpp>
#include <cereal/types/optional.hpp>
#include <cxxopts.hpp>
#include <fmt/core.h>
#include <fstream>
#include <spdlog/common.h>

namespace praas::control_plane::config {

  void HTTPServer::load(cereal::JSONInputArchive& archive)
  {
    // All arguments are optional

    archive(CEREAL_NVP(threads));
    archive(CEREAL_NVP(port));
    archive(CEREAL_NVP(enable_ssl));

    if (this->enable_ssl) {
      std::string ssl_server_key;
      std::string ssl_server_cert;
      archive(CEREAL_NVP(ssl_server_key));
      archive(CEREAL_NVP(ssl_server_cert));

      this->ssl_server_key = std::move(ssl_server_key);
      this->ssl_server_cert = std::move(ssl_server_cert);
    }
  }

  void HTTPServer::set_defaults()
  {
    threads = DEFAULT_THREADS_NUMBER;
    enable_ssl = false;
    port = DEFAULT_PORT;
    // 1 MiB
    max_payload_size = 1024 * 1024;
  }

  void Workers::load(cereal::JSONInputArchive& archive)
  {
    archive(CEREAL_NVP(threads));
  }

  void Workers::set_defaults()
  {
    threads = DEFAULT_THREADS_NUMBER;
  }

  void DownScaler::load(cereal::JSONInputArchive& archive)
  {
    archive(CEREAL_NVP(polling_interval));
    archive(CEREAL_NVP(swapping_threshold));
  }

  void DownScaler::set_defaults()
  {
    polling_interval = DEFAULT_POLLING_INTERVAL;
    swapping_threshold = DEFAULT_SWAPPING_THRESHOLD;
  }

  void TCPServer::load(cereal::JSONInputArchive& archive)
  {
    archive(CEREAL_NVP(port));
    archive(CEREAL_NVP(io_threads));
  }

  void TCPServer::set_defaults()
  {
    port = DEFAULT_PORT;
    io_threads = 1;
  }

  void BackendDocker::load(cereal::JSONInputArchive& archive)
  {
    archive(CEREAL_NVP(address));
    archive(CEREAL_NVP(port));
  }

  void BackendDocker::set_defaults()
  {
    address = "127.0.0.1";
    port = 8080;
  }

  void BackendFargate::load(cereal::JSONInputArchive& archive)
  {
    archive(CEREAL_NVP(fargate_config));
  }

  void BackendFargate::set_defaults()
  {
    fargate_config = "";
  }

  Config Config::deserialize(std::istream& in_stream)
  {
    Config cfg;
    cereal::JSONInputArchive archive_in(in_stream);
    cfg.load(archive_in);
    return cfg;
  }

  Config Config::deserialize(int argc, char** argv)
  {
    cxxopts::Options options("praas-control-plane", "Executes PraaS control plane.");
    options.add_options()("c,config", "JSON config.", cxxopts::value<std::string>());
    auto parsed_options = options.parse(argc, argv);

    std::string config_file{parsed_options["config"].as<std::string>()};

    Config cfg;
    if (config_file.length() > 0) {
      std::ifstream in_stream{config_file};
      if (!in_stream.is_open()) {
        spdlog::error("Could not open config file {}", config_file);
        exit(1);
      }

      cereal::JSONInputArchive archive_in(in_stream);
      cfg.load(archive_in);
    } else {

      cfg.set_defaults();
    }

    return cfg;
  }

  void Config::set_defaults()
  {
    backend_type = backend::Type::DOCKER;
    backend = std::make_unique<BackendDocker>();
    public_ip_address = "127.0.0.1";
    http_client_io_threads = 1;

    http.set_defaults();
    workers.set_defaults();
    down_scaler.set_defaults();
    tcpserver.set_defaults();
  }

  void Config::load(cereal::JSONInputArchive& archive)
  {
    archive(CEREAL_NVP(verbose));
    std::string backend_type;

    archive(cereal::make_nvp("backend-type", backend_type));
    this->backend_type = backend::deserialize(backend_type);
    if (this->backend_type == backend::Type::DOCKER) {
      auto ptr = std::make_unique<BackendDocker>();
      common::util::cereal_load_optional(archive, "backend", *ptr);
      this->backend = std::move(ptr);
    } else if (this->backend_type == backend::Type::AWS_FARGATE) {
      auto ptr = std::make_unique<BackendFargate>();
      common::util::cereal_load_optional(archive, "backend", *ptr);
      this->backend = std::move(ptr);
    }

    std::string deployment_type;
    archive(cereal::make_nvp("deployment-type", deployment_type));
    this->deployment_type = deployment::deserialize(deployment_type);

    archive(cereal::make_nvp("ip-address", public_ip_address));
    archive(cereal::make_nvp("http-client-io-threads", http_client_io_threads));

    common::util::cereal_load_optional(archive, "http", this->http);
    common::util::cereal_load_optional(archive, "workers", this->workers);
    common::util::cereal_load_optional(archive, "downscaler", this->down_scaler);
    common::util::cereal_load_optional(archive, "tcpserver", this->tcpserver);
  }

} // namespace praas::control_plane::config
