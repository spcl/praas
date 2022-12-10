
#include <praas/control-plane/config.hpp>
#include <praas/control-plane/server.hpp>
#include <praas/sdk/praas.hpp>

#include <chrono>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <thread>

class IntegrationLocalInvocation : public ::testing::Test {
protected:
  void SetUp() override
  {
    praas::control_plane::config::Config cfg;
    cfg.set_defaults();

    _server = std::make_unique<praas::control_plane::Server>(cfg);
  }

  std::unique_ptr<praas::control_plane::Server> _server;
};

TEST_F(IntegrationLocalInvocation, Invoke)
{
  _server->run();

  std::this_thread::sleep_for(std::chrono::milliseconds(250));

  {
    praas::sdk::PraaS praas{fmt::format("http://127.0.0.1:{}", _server->http_port())};

    EXPECT_TRUE(praas.create_application("test_app"));
  }


  _server->shutdown();
}
