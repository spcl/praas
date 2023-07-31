
#include <praas/common/exceptions.hpp>
#include <praas/control-plane/backend.hpp>
#include <praas/control-plane/config.hpp>

#include <sstream>

#include <gtest/gtest.h>

using namespace praas::control_plane::config;

TEST(Config, BasicConfig)
{
  std::string config = R"(
    {
      "verbose": true,
      "backend-type": "docker",
      "ip-address": "127.0.0.1"
    }
  )";

  std::stringstream stream{config};
  Config cfg = Config::deserialize(stream);

  EXPECT_EQ(cfg.verbose, true);
  EXPECT_EQ(cfg.backend_type, praas::control_plane::backend::Type::DOCKER);

  EXPECT_EQ(cfg.http.port, HTTPServer::DEFAULT_PORT);
  EXPECT_EQ(cfg.http.threads, HTTPServer::DEFAULT_THREADS_NUMBER);
  EXPECT_EQ(cfg.http.enable_ssl, false);

  EXPECT_EQ(cfg.workers.threads, Workers::DEFAULT_THREADS_NUMBER);

  EXPECT_EQ(cfg.down_scaler.polling_interval, DownScaler::DEFAULT_POLLING_INTERVAL);
  EXPECT_EQ(cfg.down_scaler.swapping_threshold, DownScaler::DEFAULT_SWAPPING_THRESHOLD);

  EXPECT_EQ(cfg.tcpserver.port, TCPServer::DEFAULT_PORT);
  EXPECT_EQ(cfg.tcpserver.io_threads, 1);
}

TEST(Config, HTTPConfig)
{
  {
    std::string config = R"(
      {
        "verbose": true,
        "backend-type": "docker",
        "ip-address": "127.0.0.1",
        "http": {
          "threads": 2,
          "port": 1000,
          "enable_ssl": false
        }
      }
    )";

    std::stringstream stream{config};
    Config cfg = Config::deserialize(stream);

    EXPECT_EQ(cfg.http.port, 1000);
    EXPECT_EQ(cfg.http.threads, 2);
    EXPECT_EQ(cfg.http.enable_ssl, false);
  }

  {
    std::string config = R"(
      {
        "verbose": true,
        "backend-type": "docker",
        "ip-address": "127.0.0.1",
        "http": {
          "threads": 2,
          "port": 1000,
          "enable_ssl": true,
          "ssl_server_key": "key.pem",
          "ssl_server_cert": "key.cert"
        }
      }
    )";

    std::stringstream stream{config};
    Config cfg = Config::deserialize(stream);

    EXPECT_EQ(cfg.http.port, 1000);
    EXPECT_EQ(cfg.http.threads, 2);
    EXPECT_EQ(cfg.http.enable_ssl, true);
    ASSERT_TRUE(cfg.http.ssl_server_key.has_value());
    ASSERT_TRUE(cfg.http.ssl_server_cert.has_value());
    EXPECT_EQ(cfg.http.ssl_server_key, "key.pem");
    EXPECT_EQ(cfg.http.ssl_server_cert, "key.cert");
  }

  {
    std::string config = R"(
      {
        "verbose": true,
        "backend-type": "docker",
        "ip-address": "127.0.0.1",
        "http": {
          "threads": 2,
          "port": 1000,
          "enable_ssl": true
        }
      }
    )";

    std::stringstream stream{config};
    EXPECT_THROW(Config::deserialize(stream), praas::common::InvalidConfigurationError);
  }
}

TEST(Config, WorkersConfig)
{
  {
    std::string config = R"(
      {
        "verbose": true,
        "backend-type": "docker",
        "ip-address": "127.0.0.1",
        "workers": {
          "threads": 4
        }
      }
    )";

    std::stringstream stream{config};
    Config cfg = Config::deserialize(stream);

    EXPECT_EQ(cfg.workers.threads, 4);
  }

  {
    std::string config = R"(
      {
        "verbose": true,
        "backend-type": "docker",
        "ip-address": "127.0.0.1",
        "workers": {
        }
      }
    )";

    std::stringstream stream{config};
    EXPECT_THROW(Config::deserialize(stream), praas::common::InvalidConfigurationError);
  }
}

TEST(Config, DownScalerConfig)
{
  {
    std::string config = R"(
      {
        "verbose": true,
        "backend-type": "docker",
        "ip-address": "127.0.0.1",
        "downscaler": {
          "polling_interval": 30,
          "swapping_threshold": 60
        }
      }
    )";

    std::stringstream stream{config};
    Config cfg = Config::deserialize(stream);

    EXPECT_EQ(cfg.down_scaler.swapping_threshold, 60);
    EXPECT_EQ(cfg.down_scaler.polling_interval, 30);
  }

  {
    std::string config = R"(
      {
        "verbose": true,
        "backend-type": "docker",
        "ip-address": "127.0.0.1",
        "downscaler": {
          "polling_interval": 30
        }
      }
    )";

    std::stringstream stream{config};
    EXPECT_THROW(Config::deserialize(stream), praas::common::InvalidConfigurationError);
  }
}

TEST(Config, TCPServerConfig)
{
  {
    std::string config = R"(
      {
        "verbose": true,
        "backend-type": "docker",
        "ip-address": "127.0.0.1",
        "tcpserver": {
          "port": 2000,
          "io_threads": 5
        }
      }
    )";

    std::stringstream stream{config};
    Config cfg = Config::deserialize(stream);

    EXPECT_EQ(cfg.tcpserver.port, 2000);
    EXPECT_EQ(cfg.tcpserver.io_threads, 5);
  }

  {
    std::string config = R"(
      {
        "verbose": true,
        "backend-type": "docker",
        "ip-address": "127.0.0.1",
        "tcpserver": {
        }
      }
    )";

    std::stringstream stream{config};
    EXPECT_THROW(Config::deserialize(stream), praas::common::InvalidConfigurationError);
  }
}
