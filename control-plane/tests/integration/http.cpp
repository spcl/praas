
#include <praas/common/exceptions.hpp>
#include <praas/common/messages.hpp>
#include <praas/control-plane/backend.hpp>
#include <praas/control-plane/config.hpp>
#include <praas/control-plane/http.hpp>
#include <praas/control-plane/deployment.hpp>
#include <praas/control-plane/process.hpp>
#include <praas/control-plane/resources.hpp>
#include <praas/control-plane/tcpserver.hpp>
#include <praas/control-plane/worker.hpp>

#include <drogon/HttpTypes.h>
#include <thread>

#include <gmock/gmock-actions.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sockpp/tcp_connector.h>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>
#include <trantor/net/EventLoopThread.h>
#include <variant>

using namespace praas::control_plane;

class MockWorkers : public worker::Workers {
public:
  MockWorkers(Resources& resources, backend::Backend & backend):
    worker::Workers(config::Workers{}, backend, resources)
  {}
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

class HttpTCPIntegration : public ::testing::Test {
protected:
  void SetUp() override
  {
    //ON_CALL(backend, max_memory()).WillByDefault(testing::Return(4096));
    //ON_CALL(backend, max_vcpus()).WillByDefault(testing::Return(4));

    workers.attach_tcpserver(server);

    spdlog::set_level(spdlog::level::debug);
  }

  Resources resources;
  MockBackend backend;
  MockWorkers workers{resources, backend};
  MockDeployment deployment;

  config::TCPServer config;
  tcpserver::TCPServer server{config, workers};
};

TEST_F(HttpTCPIntegration, Invoke)
{
  // FIXME: config structure for http
  config::HTTPServer cfg;
  auto http_server = std::make_shared<HttpServer>(cfg, workers);
  http_server->run();

  trantor::EventLoopThread _loop;
  _loop.run();

  std::this_thread::sleep_for(std::chrono::milliseconds(250));

  // Connect to the HTTP Server
  auto client = drogon::HttpClient::newHttpClient(
    fmt::format("http://127.0.0.1:{}/", cfg.port),
    _loop.getLoop(), false, false
  );

  // Create the application.
  {
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath("/create_app");
    req->setParameter("name", "app_id");

    std::promise<void> p;
    client->sendRequest(req,
      [&p](drogon::ReqResult result, const drogon::HttpResponsePtr &response) {
        EXPECT_EQ(result, drogon::ReqResult::Ok);
        EXPECT_EQ(response.get()->getStatusCode(), drogon::k200OK);
        p.set_value();
      }
    );
    p.get_future().wait();
  }

  /**
   * Invoke the function.
   * (1) Ensure that process is created.
   * (2) Ensure that process receives invocation request.
   * (3) Ensure that reply is received at the client that made the request.
   **/
  {
    std::string resource_name{"sandbox"};
    std::string process_name{"controlplane-0"};

    std::promise<void> created_process;
    EXPECT_CALL(backend, allocate_process(testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillOnce([&](process::ProcessPtr ptr, const process::Resources&) -> void {
          ptr->handle().resource_id = resource_name;
          ptr->handle().instance_id = "id";

          EXPECT_EQ(ptr->name(), process_name);

          created_process.set_value();
        });

    std::string func_name{"func"};
    std::string invocation_data = "{\"str\": \"test\"}";
    std::string func_response{"result"};

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath(fmt::format("/invoke/{}/{}", "app_id", func_name));
    req->setBody(invocation_data);
    req->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);

    std::promise<void> p;
    client->sendRequest(req,
      [&](drogon::ReqResult result, const drogon::HttpResponsePtr &response) {

        EXPECT_EQ(result, drogon::ReqResult::Ok);
        EXPECT_EQ(response.get()->getStatusCode(), drogon::k200OK);

        auto json = response->getJsonObject();
        ASSERT_TRUE(json);
        EXPECT_EQ((*json)["function"], func_name);
        EXPECT_EQ((*json)["return_code"], 0);
        EXPECT_EQ((*json)["result"].asString(), func_response);

        p.set_value();
      }
    );

    // Wait for the request to be received
    created_process.get_future().wait();

    // Connect the process to control plane
    sockpp::tcp_connector process_socket;
    ASSERT_TRUE(process_socket.connect(sockpp::inet_address("localhost", server.port())));

    // Registration
    praas::common::message::ProcessConnection msg;
    msg.process_name(process_name);
    process_socket.write_n(msg.bytes(), decltype(msg)::BUF_SIZE);

    // Ensure that we receive the invocation requests
    praas::common::message::Message recv_msg;
    process_socket.read_n(recv_msg.data.data(), decltype(msg)::BUF_SIZE);

    // Receive invocation data
    auto parsed_msg = recv_msg.parse();
    ASSERT_TRUE(std::holds_alternative<praas::common::message::InvocationRequestParsed>(parsed_msg));
    auto invoc_msg = std::get<praas::common::message::InvocationRequestParsed>(parsed_msg);
    ASSERT_EQ(invocation_data.length(), invoc_msg.total_length());

    std::unique_ptr<char[]> buf{new char[invoc_msg.total_length()]};
    process_socket.read_n(buf.get(), invoc_msg.total_length());

    // Reply with response
    praas::common::message::InvocationResult result;
    result.invocation_id(invoc_msg.invocation_id());
    result.return_code(0);
    result.total_length(func_response.length());
    process_socket.write_n(result.bytes(), decltype(msg)::BUF_SIZE);
    process_socket.write_n(func_response.data(), func_response.length());

    p.get_future().wait();

    // Second invocation - should use the same process.

    invocation_data = std::string{"{\"str\": \"test2\"}"};
    func_response = std::string{"result2"};
    p = std::promise<void>{};

    req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath(fmt::format("/invoke/{}/{}", "app_id", func_name));
    req->setBody(invocation_data);
    req->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);
    client->sendRequest(req,
      [&](drogon::ReqResult result, const drogon::HttpResponsePtr &response) {

        EXPECT_EQ(result, drogon::ReqResult::Ok);
        EXPECT_EQ(response.get()->getStatusCode(), drogon::k200OK);

        auto json = response->getJsonObject();
        ASSERT_TRUE(json);
        EXPECT_EQ((*json)["function"], func_name);
        EXPECT_EQ((*json)["return_code"], 0);
        EXPECT_EQ((*json)["result"].asString(), func_response);

        p.set_value();
      }
    );

    // Receive invocation data
    recv_msg = praas::common::message::Message{};
    process_socket.read_n(recv_msg.data.data(), decltype(msg)::BUF_SIZE);

    parsed_msg = recv_msg.parse();
    ASSERT_TRUE(std::holds_alternative<praas::common::message::InvocationRequestParsed>(parsed_msg));
    invoc_msg = std::get<praas::common::message::InvocationRequestParsed>(parsed_msg);
    ASSERT_EQ(invocation_data.length(), invoc_msg.total_length());

    buf.reset(new char[invoc_msg.total_length()]);
    process_socket.read_n(buf.get(), invoc_msg.total_length());

    // Reply with response
    result.invocation_id(invoc_msg.invocation_id());
    result.return_code(0);
    result.total_length(func_response.length());
    process_socket.write_n(result.bytes(), decltype(result)::BUF_SIZE);
    process_socket.write_n(func_response.data(), func_response.length());

    p.get_future().wait();
  }

  /**
   * Third test - ensure that no new process is new created upon invocation.
   * Reuse the existing one.
   **/

  client.reset();

  _loop.getLoop()->quit();
  _loop.wait();

  http_server->shutdown();
}

