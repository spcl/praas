#include <praas/control-plane/process.hpp>
#include <praas/control-plane/resources.hpp>
#include <praas/control-plane/tcpserver.hpp>

#include <praas/common/exceptions.hpp>
#include <praas/control-plane/config.hpp>
#include <praas/control-plane/worker.hpp>

#include <sockpp/tcp_connector.h>
#include <tuple>

#include <sys/socket.h>

#include <gmock/gmock-actions.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace praas::control_plane;

class MockWorkers : public worker::Workers {
public:
  MockWorkers() : worker::Workers(config::Workers{}) {}
};

//class MockTCPServer : public tcpserver::TCPServer {
//public:
//  MockTCPServer(MockWorkers& workers) : tcpserver::TCPServer(config::TCPServer{}, workers)
//  {
//    ON_CALL(this, Concrete).WillByDefault([&foo](const char* str) {
//      return foo.Foo::Concrete(str);
//    });
//  }
//};

class MockBackend : public backend::Backend {
public:
  MOCK_METHOD(void, allocate_process, (process::ProcessPtr, const process::Resources&), ());
  MOCK_METHOD(int, max_memory, (), (const));
  MOCK_METHOD(int, max_vcpus, (), (const));
};

std::tuple<int, int> create_sockets()
{
  std::array<int, 2> sockets{};
  int ret = socketpair(AF_LOCAL, SOCK_STREAM | SOCK_NONBLOCK, 0, sockets.data());
  if(ret != 0) {
    spdlog::error("socketpair returned {}", ret);
    abort();
  }
  return std::make_tuple(sockets[0], sockets[1]);
}

/**
 *
 * Testing epoll-based TCP server. We send messages, and verify that processing method
 * is called correctly.
 * - Send each type message from one thread.
 * - Send single message type consecutively.
 * - Send messages from multiple threads to different sockets, verify they are not lost.
 * - Verify error handling - send incomplete message and close connection.
 * - Send incorrect message - handle error.
 *
 * Testing with multiple threads:
 * - Enable thread pool.
 * - Verify that messages are not reordered - same client, multiple messages, make sure that
 *   everything is correctly processed.
 *
 * Specific testing of messages
 * - Register process
 * - Incorrect registration of process - unknown name
 * - Different message except for acceptance.
 *
 * Delete process while server is still holding a reference.
 *
 * Delete process and verify that epoll is smaller.
 */

TEST(TCPServerTest, SingleThreadRegister)
{
  spdlog::set_pattern("*** [%H:%M:%S %z] [thread %t] %v ***");

  MockWorkers workers;
  MockBackend backend;
  Application app;
  std::string resource_name{"sandbox"};
  std::string process_name{"sandbox"};
  process::Resources resources{1, 128, resource_name};
  int port = 10000;

  config::TCPServer cfg;
  cfg.port = port;
  tcpserver::TCPServer server{cfg, workers, true};
  std::thread worker{&tcpserver::TCPServer::start, &server};

  auto process = std::make_shared<process::Process>(process_name, &app, std::move(resources));
  server.add_process(process::ProcessObserver{process});

  sockpp::tcp_connector process_socket;
  ASSERT_TRUE(process_socket.connect(sockpp::inet_address("localhost", port)));
  EXPECT_EQ(server.num_connected_processes(), 1);
  EXPECT_EQ(server.num_registered_processes(), 0);

  // Send registration message
  praas::common::message::ProcessConnection msg;
  msg.process_name() = process_name;
  process_socket.write_n(&msg, decltype(msg)::MSG_SIZE);

  // Verify that the process has been registered
  EXPECT_EQ(server.num_registered_processes(), 1);

  server.shutdown();
  worker.join();
}
