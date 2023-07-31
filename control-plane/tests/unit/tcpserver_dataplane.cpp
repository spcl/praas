
#include "../mocks.hpp"

#include <praas/common/exceptions.hpp>
#include <praas/common/messages.hpp>
#include <praas/control-plane/config.hpp>
#include <praas/control-plane/process.hpp>

#include <chrono>
#include <ratio>
#include <thread>

#include <gtest/gtest.h>
#include <sockpp/tcp_connector.h>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>

class TCPServerTest : public ::testing::Test {
protected:
  void SetUp() override
  {
    _app_create = Application{"app", ApplicationResources{}};

    setup_mocks(backend);

    spdlog::set_pattern("*** [%H:%M:%S %z] [thread %t] %v ***");
    spdlog::set_level(spdlog::level::debug);
  }

  Application _app_create;
  MockBackend backend;
  MockWorkers workers{backend, deployment};
  MockDeployment deployment;
};

/**
 *
 * (1) Connect and send data plane metrics. Check it has been stored.
 * (2) Send metrics without registration. Check it has been handled without error.
 *
 */

TEST_F(TCPServerTest, UpdateMetrics)
{
  std::string resource_name{"sandbox"};
  std::string process_name{"sandbox"};
  process::Resources resources{1, 128, resource_name};
  Application app;

  config::TCPServer config;
  config.set_defaults();

  praas::control_plane::tcpserver::TCPServer server(config, workers);
  int port = server.port();

  auto process = std::make_shared<process::Process>(process_name, &app, std::move(resources));
  server.add_process(process);

  sockpp::tcp_connector process_socket;
  ASSERT_TRUE(process_socket.connect(sockpp::inet_address("localhost", port)));

  // Correct registration
  praas::common::message::ProcessConnection msg;
  msg.process_name(process_name);
  process_socket.write_n(msg.bytes(), decltype(msg)::BUF_SIZE);

  // Send data plane metrics
  praas::common::message::DataPlaneMetrics metrics_msg;
  metrics_msg.computation_time(2000);
  metrics_msg.invocations(5);
  auto timestamp = std::chrono::system_clock::now();
  metrics_msg.last_invocation_timestamp(timestamp.time_since_epoch().count());

  process_socket.write_n(metrics_msg.bytes(), decltype(msg)::BUF_SIZE);
  // Wait until the callback is processed
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  praas::control_plane::process::DataPlaneMetrics metrics = process->get_metrics();
  EXPECT_EQ(metrics.computation_time, 2000);
  EXPECT_EQ(metrics.invocations, 5);
  EXPECT_EQ(metrics.last_invocation, timestamp.time_since_epoch().count());

  process_socket.close();

  server.shutdown();
}

TEST_F(TCPServerTest, UpdateMetricsIncorrect)
{
  std::string resource_name{"sandbox"};
  std::string process_name{"sandbox"};
  process::Resources resources{1, 128, resource_name};
  Application app;

  config::TCPServer config;
  config.set_defaults();

  praas::control_plane::tcpserver::TCPServer server(config, workers);
  int port = server.port();

  auto process = std::make_shared<process::Process>(process_name, &app, std::move(resources));
  server.add_process(process);

  sockpp::tcp_connector process_socket;
  ASSERT_TRUE(process_socket.connect(sockpp::inet_address("localhost", port)));

  // Send data plane metrics without registration
  praas::common::message::DataPlaneMetrics metrics_msg;
  metrics_msg.computation_time(2000);
  metrics_msg.invocations(5);
  auto timestamp = std::chrono::system_clock::now();
  metrics_msg.last_invocation_timestamp(timestamp.time_since_epoch().count());

  process_socket.write_n(metrics_msg.bytes(), decltype(metrics_msg)::BUF_SIZE);
  // Wait until the callback is processed
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  praas::control_plane::process::DataPlaneMetrics metrics = process->get_metrics();
  EXPECT_EQ(metrics.computation_time, 0);
  EXPECT_EQ(metrics.invocations, 0);
  EXPECT_EQ(metrics.last_invocation, 0);

  process_socket.close();

  server.shutdown();
}
