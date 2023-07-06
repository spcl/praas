
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

    ON_CALL(backend, max_memory()).WillByDefault(testing::Return(4096));
    ON_CALL(backend, max_vcpus()).WillByDefault(testing::Return(4));
  }

  void TearDown() override
  {
    praas::common::http::HTTPClientFactory::shutdown();
  }

  std::unique_ptr<state::DiskSwapLocation> swap_loc{new state::DiskSwapLocation{"loc"}};
  Resources resources;
  MockBackend backend;
  MockDeployment deployment;
  worker::Workers workers{config::Workers{}, backend, deployment, resources};

  config::TCPServer config;
  tcpserver::TCPServer server{config, workers};
};

TEST_F(HttpServerTest, CreateApp)
{
  config::HTTPServer cfg;
  auto http_server = std::make_shared<HttpServer>(cfg, workers);
  http_server->run();

  std::this_thread::sleep_for(std::chrono::milliseconds(250));

  // Connect to the HTTP Server
  auto client =
      praas::common::http::HTTPClientFactory::create_client_shared("http://127.0.0.1", cfg.port);

  // Create the application.
  {
    std::promise<void> p;
    client.put(
        fmt::format("/apps/{}", "app_id"), {{"cloud_resource_name", "container_42"}},
        [&p](drogon::ReqResult result, const drogon::HttpResponsePtr& response) {
          EXPECT_EQ(result, drogon::ReqResult::Ok);
          EXPECT_EQ(response.get()->getStatusCode(), drogon::k200OK);
          std::cerr << response.get()->getBody() << std::endl;
          p.set_value();
        }
    );
    p.get_future().wait();

    Resources::ROAccessor acc;
    resources.get_application("app_id", acc);
    EXPECT_FALSE(acc.empty());
  }

  http_server->shutdown();
}

TEST_F(HttpServerTest, CreateProcess)
{
  config::HTTPServer cfg;
  auto http_server = std::make_shared<HttpServer>(cfg, workers);
  http_server->run();

  std::this_thread::sleep_for(std::chrono::milliseconds(250));

  // Connect to the HTTP Server
  auto client =
      praas::common::http::HTTPClientFactory::create_client_shared("http://127.0.0.1", cfg.port);

  // Create the application.
  {
    std::promise<void> promise_app, promise_proc;
    client.put(
        fmt::format("/apps/{}", "app_id42"), {{"cloud_resource_name", "container_42"}},
        [&promise_app](drogon::ReqResult result, const drogon::HttpResponsePtr& response) {
          EXPECT_EQ(result, drogon::ReqResult::Ok);
          EXPECT_EQ(response.get()->getStatusCode(), drogon::k200OK);
          promise_app.set_value();
        }
    );
    client.put(
        fmt::format("/apps/{}/processes/{}", "app_id42", "proc_id42"),
        {{"vcpus", "1"}, {"memory", "1024"}},
        [&promise_proc](drogon::ReqResult result, const drogon::HttpResponsePtr& response) {
          EXPECT_EQ(result, drogon::ReqResult::Ok);
          EXPECT_EQ(response.get()->getStatusCode(), drogon::k200OK);
          std::cerr << response.get()->getBody() << std::endl;
          promise_proc.set_value();
        }
    );
    promise_app.get_future().wait();
    promise_proc.get_future().wait();

    Resources::ROAccessor acc;
    resources.get_application("app_id42", acc);
    ASSERT_FALSE(acc.empty());
    EXPECT_EQ(acc.get()->name(), "app_id42");
    auto [lock, proc] = acc.get()->get_process("proc_id42");
    ASSERT_TRUE(proc != nullptr);
    EXPECT_EQ(proc->name(), "proc_id42");
  }

  http_server->shutdown();
}

TEST_F(HttpServerTest, DeleteProcess)
{
  config::HTTPServer cfg;
  auto http_server = std::make_shared<HttpServer>(cfg, workers);
  http_server->run();

  std::this_thread::sleep_for(std::chrono::milliseconds(250));

  // Connect to the HTTP Server
  auto client =
      praas::common::http::HTTPClientFactory::create_client_shared("http://127.0.0.1", cfg.port);

  std::string app_name = "test_app_42";
  std::string app_resource = "test_container";
  std::string proc_name = "test_proc_42";

  {
    Resources::RWAccessor app_acc;
    resources.add_application(Application(app_name, ApplicationResources(app_resource)));
    resources.get_application(app_name, app_acc);
    ASSERT_FALSE(app_acc.empty());

    app_acc.get()->add_process(backend, server, proc_name, process::Resources(1, 2048, ""));
  }

  // Now delete the application - this should fail as the process is not active.
  {
    std::promise<void> promise_proc;
    client.post(
        fmt::format("/apps/{}/processes/{}/delete", app_name, proc_name), {},
        [&promise_proc](drogon::ReqResult result, const drogon::HttpResponsePtr& response) {
          EXPECT_EQ(result, drogon::ReqResult::Ok);
          EXPECT_EQ(response.get()->getStatusCode(), drogon::HttpStatusCode::k400BadRequest);
          promise_proc.set_value();
        }
    );
    promise_proc.get_future().wait();
  }

  EXPECT_CALL(deployment, get_location(testing::_))
      .WillOnce(testing::Return(testing::ByMove(std::move(swap_loc))));

  {
    Resources::RWAccessor app_acc;
    resources.get_application(app_name, app_acc);
    ASSERT_FALSE(app_acc.empty());

    // FIXME: This should be encapsulated in a routine.
    // Manually change the process to be allocated.
    {
      auto [lock, proc] = app_acc.get()->get_process(proc_name);
      proc->set_status(process::Status::ALLOCATED);
    }

    app_acc.get()->swap_process(proc_name, deployment);
    app_acc.get()->swapped_process(proc_name);
  }

  //// Now delete the application again - this should succeed as the process is swapped.
  {
    std::promise<void> promise_proc;
    client.post(
        fmt::format("/apps/{}/processes/{}/delete", app_name, proc_name), {},
        [&promise_proc](drogon::ReqResult result, const drogon::HttpResponsePtr& response) {
          EXPECT_EQ(result, drogon::ReqResult::Ok);
          EXPECT_EQ(response.get()->getStatusCode(), drogon::k200OK);
          promise_proc.set_value();
        }
    );
    promise_proc.get_future().wait();

    Resources::ROAccessor app_acc;
    resources.get_application(app_name, app_acc);
    ASSERT_FALSE(app_acc.empty());

    // The process does not exist anymore
    EXPECT_THROW(app_acc.get()->get_swapped_process(proc_name), praas::common::ObjectDoesNotExist);
  }

  http_server->shutdown();
}

TEST_F(HttpServerTest, SwapProcess)
{
  config::HTTPServer cfg;
  auto http_server = std::make_shared<HttpServer>(cfg, workers);
  http_server->run();

  std::this_thread::sleep_for(std::chrono::milliseconds(250));

  // Connect to the HTTP Server
  auto client =
      praas::common::http::HTTPClientFactory::create_client_shared("http://127.0.0.1", cfg.port);

  std::string app_name = "test_app_42";
  std::string app_resource = "test_container";
  std::string proc_name = "test_proc_42";

  {
    Resources::RWAccessor app_acc;
    resources.add_application(Application(app_name, ApplicationResources(app_resource)));
    resources.get_application(app_name, app_acc);
    ASSERT_FALSE(app_acc.empty());

    app_acc.get()->add_process(backend, server, proc_name, process::Resources(1, 2048, ""));
  }

  // Now swap the process - should fail because process is not allocated.
  {
    std::promise<void> promise_proc;
    client.post(
        fmt::format("/apps/{}/processes/{}/swap", app_name, proc_name), {},
        [&promise_proc](drogon::ReqResult result, const drogon::HttpResponsePtr& response) {
          EXPECT_EQ(result, drogon::ReqResult::Ok);
          EXPECT_EQ(response.get()->getStatusCode(), drogon::HttpStatusCode::k400BadRequest);
          promise_proc.set_value();
        }
    );
    promise_proc.get_future().wait();
  }

  // Now swap the process - should fail because process does not exist.
  {
    std::promise<void> promise_proc;
    client.post(
        fmt::format("/apps/{}/processes/{}_invalid/swap", app_name, proc_name), {},
        [&promise_proc](drogon::ReqResult result, const drogon::HttpResponsePtr& response) {
          EXPECT_EQ(result, drogon::ReqResult::Ok);
          EXPECT_EQ(response.get()->getStatusCode(), drogon::HttpStatusCode::k400BadRequest);
          promise_proc.set_value();
        }
    );
    promise_proc.get_future().wait();
  }

  EXPECT_CALL(deployment, get_location(testing::_))
      .WillOnce(testing::Return(testing::ByMove(std::move(swap_loc))));

  {
    Resources::RWAccessor app_acc;
    resources.get_application(app_name, app_acc);
    ASSERT_FALSE(app_acc.empty());

    // FIXME: This should be encapsulated in a routine.
    // Manually change the process to be allocated.
    {
      auto [lock, proc] = app_acc.get()->get_process(proc_name);
      proc->set_status(process::Status::ALLOCATED);
    }
  }

  // Now swapping should work.
  {
    std::promise<void> promise_proc;
    client.post(
        fmt::format("/apps/{}/processes/{}/swap", app_name, proc_name), {},
        [&promise_proc](drogon::ReqResult result, const drogon::HttpResponsePtr& response) {
          EXPECT_EQ(result, drogon::ReqResult::Ok);
          EXPECT_EQ(response.get()->getStatusCode(), drogon::k200OK);
          promise_proc.set_value();
        }
    );
    promise_proc.get_future().wait();

    Resources::ROAccessor app_acc;
    resources.get_application(app_name, app_acc);
    ASSERT_FALSE(app_acc.empty());

    // The process is being swapped out.
    EXPECT_THROW(app_acc.get()->get_swapped_process(proc_name), praas::common::ObjectDoesNotExist);
    auto [lock, proc] = app_acc.get()->get_process(proc_name);
    ASSERT_TRUE(proc != nullptr);
    EXPECT_EQ(proc->status(), process::Status::SWAPPING_OUT);
  }

  // Swapping again should fail.
  {
    std::promise<void> promise_proc;
    client.post(
        fmt::format("/apps/{}/processes/{}/swap", app_name, proc_name), {},
        [&promise_proc](drogon::ReqResult result, const drogon::HttpResponsePtr& response) {
          EXPECT_EQ(result, drogon::ReqResult::Ok);
          EXPECT_EQ(response.get()->getStatusCode(), drogon::HttpStatusCode::k400BadRequest);
          promise_proc.set_value();
        }
    );
    promise_proc.get_future().wait();
  }

  http_server->shutdown();
}
