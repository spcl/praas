
#include "../mocks.hpp"

#include <praas/common/exceptions.hpp>
#include <praas/common/messages.hpp>
#include <praas/control-plane/deployment.hpp>
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
    app = Application{"app", ApplicationResources{}};

    setup_mocks(backend);

    spdlog::set_pattern("*** [%H:%M:%S %z] [thread %t] %v ***");
    spdlog::set_level(spdlog::level::debug);
  }

  Application app;
  MockBackend backend;
  MockDeployment deployment;
  MockWorkers workers{backend, deployment};
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

  app.add_process(backend, server, process_name, std::move(resources), false);

  sockpp::tcp_connector process_socket;
  ASSERT_TRUE(process_socket.connect(sockpp::inet_address("localhost", port)));

  // Correct registration
  praas::common::message::ProcessConnectionData msg;
  msg.process_name(process_name);
  process_socket.write_n(msg.bytes(), decltype(msg)::BUF_SIZE);

  // Wait for the registration
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Swap
  app.swap_process(process_name, deployment);

  //// Now verify it is received
  praas::common::message::MessageData recv_msg;
  process_socket.read_n(recv_msg.data(), decltype(msg)::BUF_SIZE);

  auto received_msg = praas::common::message::MessageParser::parse(recv_msg);
  ASSERT_TRUE(std::holds_alternative<praas::common::message::SwapRequestPtr>(received_msg));
  auto parsed_msg = std::get<praas::common::message::SwapRequestPtr>(received_msg);
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

  app.add_process(backend, server, process_name, std::move(resources), false);

  sockpp::tcp_connector process_socket;
  ASSERT_TRUE(process_socket.connect(sockpp::inet_address("localhost", port)));

  // Correct registration
  praas::common::message::ProcessConnectionData msg;
  msg.process_name(process_name);
  process_socket.write_n(msg.bytes(), decltype(msg)::BUF_SIZE);

  // Wait for the registration
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Swap
  app.swap_process(process_name, deployment);

  //// Now verify it is received
  praas::common::message::MessageData recv_msg;
  process_socket.read_n(recv_msg.data(), decltype(msg)::BUF_SIZE);

  auto received_msg = praas::common::message::MessageParser::parse(recv_msg);
  ASSERT_TRUE(std::holds_alternative<praas::common::message::SwapRequestPtr>(received_msg));

  //// Reply to the system that we swapped
  praas::common::message::SwapConfirmationData conf_msg;
  conf_msg.swap_size(swap_size);
  process_socket.write_n(conf_msg.bytes(), decltype(msg)::BUF_SIZE);

  //// Wait for the swap to be processed
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  EXPECT_THROW(app.get_process(process_name), praas::common::ObjectDoesNotExist);

  {
    auto [lock, proc] = app.get_swapped_process(process_name);
    EXPECT_EQ(proc->status(), praas::control_plane::process::Status::SWAPPED_OUT);
  }

  process_socket.close();

  server.shutdown();
}
