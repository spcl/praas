
#include <praas/common/exceptions.hpp>
#include <praas/common/messages.hpp>
#include <praas/control-plane/backend.hpp>
#include <praas/control-plane/config.hpp>
#include <praas/control-plane/deployment.hpp>
#include <praas/control-plane/process.hpp>
#include <praas/control-plane/resources.hpp>
#include <praas/control-plane/tcpserver.hpp>
#include <praas/control-plane/worker.hpp>

#include <gmock/gmock-actions.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sockpp/tcp_connector.h>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>
#include <thread>

using namespace praas::control_plane;

class MockWorkers : public worker::Workers {
public:
  MockWorkers() : worker::Workers(config::Workers{}) {}
};

class MockDeployment : public deployment::Deployment {
public:
  MOCK_METHOD(std::unique_ptr<state::SwapLocation>, get_location, (std::string), (override));
  MOCK_METHOD(void, delete_swap, (const state::SwapLocation&), (override));
};

class MockBackend : public backend::Backend {
public:
  MOCK_METHOD(void, allocate_process, (process::ProcessPtr, const process::Resources&), ());
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
  MockWorkers workers;
  MockDeployment deployment;
};

TEST_F(TCPServerTest, StartServer)
{
  int PORT = 10000;

  config::TCPServer config;
  config.set_defaults();
  config.port = PORT;

  praas::control_plane::tcpserver::TCPServer server(config, workers);

  EXPECT_EQ(server.port(), PORT);

  server.shutdown();
}

TEST_F(TCPServerTest, ConnectProcess)
{

  config::TCPServer config;
  config.set_defaults();

  praas::control_plane::tcpserver::TCPServer server(config, workers);
  int port = server.port();

  sockpp::tcp_connector process_socket;
  ASSERT_TRUE(process_socket.connect(sockpp::inet_address("localhost", port)));
  // Wait until the callback is processed
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_EQ(server.num_connected_processes(), 1);
  EXPECT_EQ(server.num_registered_processes(), 0);

  process_socket.close();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_EQ(server.num_connected_processes(), 0);
  EXPECT_EQ(server.num_registered_processes(), 0);

  server.shutdown();
}

TEST_F(TCPServerTest, RegisterProcess)
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
  EXPECT_EQ(process->status(), praas::control_plane::process::Status::ALLOCATING);

  sockpp::tcp_connector process_socket;
  ASSERT_TRUE(process_socket.connect(sockpp::inet_address("localhost", port)));

  // Correct registration
  praas::common::message::ProcessConnection msg;
  msg.process_name(process_name);
  process_socket.write_n(msg.bytes(), decltype(msg)::BUF_SIZE);

  // Wait for registration to finish.
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_EQ(server.num_registered_processes(), 1);
  {
    process->read_lock();
    EXPECT_EQ(process->status(), praas::control_plane::process::Status::ALLOCATED);
  }

  process_socket.close();

  server.shutdown();
}

TEST_F(TCPServerTest, RegisterProcessIncorrect)
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
  EXPECT_EQ(process->status(), praas::control_plane::process::Status::ALLOCATING);

  sockpp::tcp_connector process_socket;
  ASSERT_TRUE(process_socket.connect(sockpp::inet_address("localhost", port)));

  praas::common::message::ProcessConnection msg;
  msg.process_name("");
  process_socket.write_n(msg.bytes(), decltype(msg)::BUF_SIZE);

  // Wait for registration to finish.
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_EQ(server.num_registered_processes(), 0);

  process_socket.close();

  server.shutdown();
}

TEST_F(TCPServerTest, RegisterProcessPartial)
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
  EXPECT_EQ(process->status(), praas::control_plane::process::Status::ALLOCATING);

  sockpp::tcp_connector process_socket;
  ASSERT_TRUE(process_socket.connect(sockpp::inet_address("localhost", port)));

  // Register
  praas::common::message::ProcessConnection msg;
  msg.process_name(process_name);
  process_socket.write_n(msg.bytes(), 8);

  // Let the handler run with partial data
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  process_socket.write_n(msg.bytes() + 8, decltype(msg)::BUF_SIZE - 8);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_EQ(server.num_registered_processes(), 1);
  {
    process->read_lock();
    EXPECT_EQ(process->status(), praas::control_plane::process::Status::ALLOCATED);
  }

  process_socket.close();

  server.shutdown();
}

TEST_F(TCPServerTest, DeregisterProcess)
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
  EXPECT_EQ(process->status(), praas::control_plane::process::Status::ALLOCATING);

  sockpp::tcp_connector process_socket;
  ASSERT_TRUE(process_socket.connect(sockpp::inet_address("localhost", port)));

  // Register
  praas::common::message::ProcessConnection msg;
  msg.process_name(process_name);
  process_socket.write_n(msg.bytes(), decltype(msg)::BUF_SIZE);

  // Deregister
  praas::common::message::ProcessClosure close_msg;
  process_socket.write_n(close_msg.bytes(), decltype(msg)::BUF_SIZE);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_EQ(server.num_registered_processes(), 0);
  EXPECT_EQ(server.num_connected_processes(), 0);
  {
    process->read_lock();
    EXPECT_EQ(process->status(), praas::control_plane::process::Status::FAILURE);
  }

  process_socket.close();

  server.shutdown();
}

TEST_F(TCPServerTest, ClosedProcess)
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
  EXPECT_EQ(process->status(), praas::control_plane::process::Status::ALLOCATING);

  sockpp::tcp_connector process_socket;
  ASSERT_TRUE(process_socket.connect(sockpp::inet_address("localhost", port)));

  // Register
  praas::common::message::ProcessConnection msg;
  msg.process_name(process_name);
  process_socket.write_n(msg.bytes(), decltype(msg)::BUF_SIZE);

  // Close - should lead to an failure state
  process_socket.close();

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_EQ(server.num_registered_processes(), 0);
  EXPECT_EQ(server.num_connected_processes(), 0);
  {
    process->read_lock();
    EXPECT_EQ(process->status(), praas::control_plane::process::Status::FAILURE);
  }

  server.shutdown();
}

