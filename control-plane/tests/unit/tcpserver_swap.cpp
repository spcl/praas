
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

class MockBackend : public backend::Backend {
public:
  MOCK_METHOD(std::shared_ptr<backend::ProcessInstance>, allocate_process, (process::ProcessPtr, const process::Resources&), ());
  MOCK_METHOD(void, shutdown, (const std::shared_ptr<backend::ProcessInstance>&), ());
  MOCK_METHOD(int, max_memory, (), (const));
  MOCK_METHOD(int, max_vcpus, (), (const));
};

class TCPServerTest : public ::testing::Test {
protected:
  void SetUp() override
  {
    app = Application{"app"};

    ON_CALL(backend, max_memory()).WillByDefault(testing::Return(4096));
    ON_CALL(backend, max_vcpus()).WillByDefault(testing::Return(4));

    spdlog::set_pattern("*** [%H:%M:%S %z] [thread %t] %v ***");
    spdlog::set_level(spdlog::level::debug);
  }

  Application app;
  MockBackend backend;
  MockWorkers workers{backend};
};

/**
 *
 * (1) Send swap request to a process, verify it is received.
 * (2) Send swap request, reply from a process, verify it is received.
 *
 */

TEST_F(TCPServerTest, SwapProcess)
{
  std::string resource_name{"sandbox"};
  std::string process_name{"sandbox"};
  std::string swap_loc{"swaps"};
  process::Resources resources{1, 128, resource_name};
  deployment::Local deployment{swap_loc};

  config::TCPServer config;
  config.set_defaults();

  praas::control_plane::tcpserver::TCPServer server(config, workers);
  int port = server.port();

  app.add_process(backend, server, process_name, std::move(resources));

  sockpp::tcp_connector process_socket;
  ASSERT_TRUE(process_socket.connect(sockpp::inet_address("localhost", port)));

  // Correct registration
  praas::common::message::ProcessConnection msg;
  msg.process_name(process_name);
  process_socket.write_n(msg.bytes(), decltype(msg)::BUF_SIZE);

  // Wait for the registration
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Swap
  app.swap_process(process_name, deployment);

  //// Now verify it is received
  praas::common::message::Message recv_msg;
  process_socket.read_n(recv_msg.data.data(), decltype(msg)::BUF_SIZE);

  auto received_msg = recv_msg.parse();

  ASSERT_TRUE(std::holds_alternative<praas::common::message::SwapRequestParsed>(received_msg));
  auto parsed_msg = std::get<praas::common::message::SwapRequestParsed>(received_msg);
  EXPECT_EQ(swap_loc, parsed_msg.path());

  process_socket.close();

  server.shutdown();
}

TEST_F(TCPServerTest, SwapProcessAndConfirm)
{
  std::string resource_name{"sandbox"};
  std::string process_name{"sandbox"};
  std::string swap_loc{"swaps"};
  int32_t swap_size = 1024;
  process::Resources resources{1, 128, resource_name};
  deployment::Local deployment{swap_loc};

  config::TCPServer config;
  config.set_defaults();

  praas::control_plane::tcpserver::TCPServer server(config, workers);
  int port = server.port();

  app.add_process(backend, server, process_name, std::move(resources));

  sockpp::tcp_connector process_socket;
  ASSERT_TRUE(process_socket.connect(sockpp::inet_address("localhost", port)));

  // Correct registration
  praas::common::message::ProcessConnection msg;
  msg.process_name(process_name);
  process_socket.write_n(msg.bytes(), decltype(msg)::BUF_SIZE);

  // Wait for the registration
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Swap
  app.swap_process(process_name, deployment);

  //// Now verify it is received
  praas::common::message::Message recv_msg;
  process_socket.read_n(recv_msg.data.data(), decltype(msg)::BUF_SIZE);

  auto received_msg = recv_msg.parse();
  ASSERT_TRUE(std::holds_alternative<praas::common::message::SwapRequestParsed>(received_msg));

  //// Reply to the system that we swapped
  praas::common::message::SwapConfirmation conf_msg;
  conf_msg.swap_size(swap_size);
  process_socket.write_n(conf_msg.bytes(), decltype(msg)::BUF_SIZE);

  //// Wait for the swap to be processed
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  EXPECT_THROW(
    app.get_process(process_name),
    praas::common::ObjectDoesNotExist
  );

  {
    auto [lock, proc] = app.get_swapped_process(process_name);
    EXPECT_EQ(proc->status(), praas::control_plane::process::Status::SWAPPED_OUT);
  }

  process_socket.close();

  server.shutdown();
}
