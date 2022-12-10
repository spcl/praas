#include <praas/common/exceptions.hpp>
#include <praas/common/util.hpp>
#include <praas/control-plane/config.hpp>

#include <optional>
#include <spdlog/common.h>
#include <stdexcept>

#include <cereal/archives/json.hpp>
#include <cereal/details/helpers.hpp>
#include <cereal/types/optional.hpp>
#include <fmt/core.h>

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

  Config Config::deserialize(std::istream& in_stream)
  {
    Config cfg;
    cereal::JSONInputArchive archive_in(in_stream);
    cfg.load(archive_in);
    return cfg;
  }

  void Config::set_defaults()
  {
    verbose = true;
    backend_type = backend::Type::LOCAL;

    http.set_defaults();
    workers.set_defaults();
    down_scaler.set_defaults();
    tcpserver.set_defaults();
  }

  void Config::load(cereal::JSONInputArchive& archive)
  {
    archive(CEREAL_NVP(verbose));

    common::util::cereal_load_optional(archive, "http", this->http);
    common::util::cereal_load_optional(archive, "workers", this->workers);
    common::util::cereal_load_optional(archive, "downscaler", this->down_scaler);
    common::util::cereal_load_optional(archive, "tcpserver", this->tcpserver);

    std::string mode;
    archive(cereal::make_nvp("backend", mode));
    backend_type = backend::deserialize(mode);
  }

} // namespace praas::control_plane::config
