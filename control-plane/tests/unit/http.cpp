
#include <praas/common/exceptions.hpp>
#include <praas/common/http.hpp>
#include <praas/common/messages.hpp>
#include <praas/control-plane/backend.hpp>
#include <praas/control-plane/config.hpp>
#include <praas/control-plane/deployment.hpp>
#include <praas/control-plane/http.hpp>
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

// class MockWorkers : public worker::Workers {
// public:
//   MockWorkers(Resources& resources, backend::Backend & backend):
//     worker::Workers(config::Workers{}, backend, resources)
//   {}
// };

class MockDeployment : public deployment::Deployment {
public:
  MOCK_METHOD(std::unique_ptr<state::SwapLocation>, get_location, (std::string), (override));
  MOCK_METHOD(void, delete_swap, (const state::SwapLocation&), (override));
};

class MockBackend : public backend::Backend {
public:
  MOCK_METHOD(
      std::shared_ptr<backend::ProcessInstance>, allocate_process,
      (process::ProcessPtr, const process::Resources&), ()
  );
  MOCK_METHOD(void, shutdown, (const std::shared_ptr<backend::ProcessInstance>&), ());
  MOCK_METHOD(int, max_memory, (), (const));
  MOCK_METHOD(int, max_vcpus, (), (const));
};

class HttpServerTest : public ::testing::Test {
protected:
  void SetUp() override
  {
    workers.attach_tcpserver(server);

    spdlog::set_level(spdlog::level::debug);

    praas::common::http::HTTPClientFactory::initialize(1);
  }

  void TearDown() override
  {
    praas::common::http::HTTPClientFactory::shutdown();
  }

  Resources resources;
  MockBackend backend;
  // MockWorkers workers{resources, backend};
  worker::Workers workers{config::Workers{}, backend, resources};
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

  //_loop.run();

  std::this_thread::sleep_for(std::chrono::milliseconds(250));

  // Connect to the HTTP Server
  auto client =
      praas::common::http::HTTPClientFactory::create_client_shared("http://127.0.0.1", cfg.port);
  //  drogon::HttpClient::newHttpClient(
  //    fmt::format("http://127.0.0.1:{}/", cfg.port), _loop.getLoop(), false, false
  //);

  // Create the application.
  {
    // auto req = drogon::HttpRequest::newHttpRequest();
    // req->setMethod(drogon::Put);
    // req->setPath(fmt::format("/apps/{}", "app_id"));
    // req->setParameter("container_name", "container_42");

    // std::promise<void> p;
    // client->sendRequest(
    //     req,
    //     [&p](drogon::ReqResult result, const drogon::HttpResponsePtr& response) {
    //       EXPECT_EQ(result, drogon::ReqResult::Ok);
    //       EXPECT_EQ(response.get()->getStatusCode(), drogon::k200OK);
    //       p.set_value();
    //     }
    //);
    // p.get_future().wait();
    std::promise<void> p;
    client.put(
        fmt::format("/apps/{}", "app_id"), {{"container_name", "container_42"}},
        [&p](drogon::ReqResult result, const drogon::HttpResponsePtr& response) {
          EXPECT_EQ(result, drogon::ReqResult::Ok);
          EXPECT_EQ(response.get()->getStatusCode(), drogon::k200OK);
          p.set_value();
        }
    );
    p.get_future().wait();

    Resources::ROAccessor acc;
    resources.get_application("app_id", acc);
    EXPECT_FALSE(acc.empty());
  }

  // client.reset();

  //_loop.getLoop()->quit();
  //_loop.wait();

  http_server->shutdown();
}
