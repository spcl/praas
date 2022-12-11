
#include <praas/common/exceptions.hpp>
#include <praas/common/messages.hpp>
#include <praas/control-plane/backend.hpp>
#include <praas/control-plane/config.hpp>
#include <praas/control-plane/deployment.hpp>
#include <praas/control-plane/process.hpp>
#include <praas/control-plane/resources.hpp>
#include <praas/control-plane/tcpserver.hpp>
#include <praas/control-plane/worker.hpp>

#include <chrono>
#include <gmock/gmock-actions.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <ratio>
#include <sockpp/tcp_connector.h>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>
#include <thread>

using namespace praas::control_plane;

class MockWorkers : public worker::Workers {
public:
  MockWorkers(backend::Backend & backend) : worker::Workers(config::Workers{}, backend, resources) {}
  Resources resources;
};

class MockDeployment : public deployment::Deployment {
public:
  MOCK_METHOD(std::unique_ptr<state::SwapLocation>, get_location, (std::string), (override));
  MOCK_METHOD(void, delete_swap, (const state::SwapLocation&), (override));
};

class MockBackend : public backend::Backend {
public:
  MOCK_METHOD(std::shared_ptr<backend::ProcessInstance>, allocate_process, (process::ProcessPtr, const process::Resources&), ());
  MOCK_METHOD(void, shutdown, (const std::shared_ptr<backend::ProcessInstance> &), ());
  MOCK_METHOD(int, max_memory, (), (const));
  MOCK_METHOD(int, max_vcpus, (), (const));
};

class TCPServerTest : public ::testing::Test {
protected:
  void SetUp() override
  {
    _app_create = Application{"app"};

    ON_CALL(backend, max_memory()).WillByDefault(testing::Return(4096));
    ON_CALL(backend, max_vcpus()).WillByDefault(testing::Return(4));

    spdlog::set_pattern("*** [%H:%M:%S %z] [thread %t] %v ***");
    spdlog::set_level(spdlog::level::debug);
  }

  Application _app_create;
  MockBackend backend;
  MockWorkers workers{backend};
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
