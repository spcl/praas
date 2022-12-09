
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

class HttpServerTest : public ::testing::Test {
protected:
  void SetUp() override
  {
    _app_create = Application{"app"};

    //ON_CALL(backend, max_memory()).WillByDefault(testing::Return(4096));
    //ON_CALL(backend, max_vcpus()).WillByDefault(testing::Return(4));

    spdlog::set_pattern("*** [%H:%M:%S %z] [thread %t] %v ***");
    spdlog::set_level(spdlog::level::debug);
  }

  Application _app_create;
  MockBackend backend;
  MockWorkers workers;
  MockDeployment deployment;
};

TEST_F(HttpServerTest, StartServer)
{
  int PORT = 10000;

  // FIXME: config structure for http
  auto server = std::make_shared<HttpServer>(workers, PORT);
  server->run();

  trantor::EventLoopThread _loop;
  _loop.run();

  std::this_thread::sleep_for(std::chrono::milliseconds(250));

  {
    Json::Value value;
    value["function"] = "no_op";
    value["invocation_id"] = "invoc-test";
    value["body"] = "datadatadatainvoc-test";
    auto client = drogon::HttpClient::newHttpClient("http://127.0.0.1:10000/", _loop.getLoop(), false, false);
    auto req = drogon::HttpRequest::newHttpJsonRequest(value);
    req->setMethod(drogon::Post);
    req->setPath("/invoke");
    req->setParameter("data", "wx");
    req->setParameter("function", "wx");
    req->setBody("{\"str\": \"ssssssss\"}");
    req->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);

    std::promise<void> p;
    client->sendRequest(req,
      [&p](drogon::ReqResult result, const drogon::HttpResponsePtr &response) {
        std::cerr << "Invocation " << result << " " << response->body() << std::endl;
        EXPECT_EQ(result, drogon::ReqResult::Ok);
        p.set_value();
      }
    );
    p.get_future().wait();
  }

  _loop.getLoop()->quit();
  _loop.wait();

  server->shutdown();
}

