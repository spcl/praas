
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

#include <thread>

#include <drogon/HttpTypes.h>
#include <gmock/gmock-actions.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sockpp/tcp_connector.h>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>
#include <trantor/net/EventLoopThread.h>

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

class HttpServerTest : public ::testing::Test {
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

TEST_F(HttpServerTest, Create)
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

  client.reset();

  _loop.getLoop()->quit();
  _loop.wait();

  http_server->shutdown();
}

