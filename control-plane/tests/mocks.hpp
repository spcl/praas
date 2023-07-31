#ifndef PRAAS_CONTROL_PLANE_MOCKS_HPP
#define PRAAS_CONTROL_PLANE_MOCKS_HPP

#include <praas/control-plane/backend.hpp>
#include <praas/control-plane/deployment.hpp>
#include <praas/control-plane/resources.hpp>
#include <praas/control-plane/tcpserver.hpp>
#include <praas/control-plane/worker.hpp>

#include <gmock/gmock-actions.h>
#include <gmock/gmock.h>

using namespace praas::control_plane;

class MockDeployment : public deployment::Deployment {
public:
  MOCK_METHOD(std::unique_ptr<state::SwapLocation>, get_location, (std::string), (override));
  MOCK_METHOD(void, delete_swap, (const state::SwapLocation&), (override));
};

class MockBackendInstance : public backend::ProcessInstance {
public:
  MOCK_METHOD(std::string, id, (), (const));
};

class MockBackend : public backend::Backend {
public:
  MOCK_METHOD(
      void, allocate_process,
      (process::ProcessPtr, const process::Resources&,
       std::function<
           void(std::shared_ptr<backend::ProcessInstance>&&, std::optional<std::string>)>&&),
      ()
  );
  MOCK_METHOD(void, shutdown, (const std::shared_ptr<backend::ProcessInstance>&), ());
  MOCK_METHOD(int, max_memory, (), (const));
  MOCK_METHOD(int, max_vcpus, (), (const));
};

class MockWorkers : public worker::Workers {
public:
  MockWorkers(backend::Backend& backend, deployment::Deployment& deployment)
      : worker::Workers(config::Workers{}, backend, deployment, resources)
  {
  }
  Resources resources;
};

class MockTCPServer : public tcpserver::TCPServer {
public:
  MockTCPServer(MockWorkers& workers) : tcpserver::TCPServer(config::TCPServer{}, workers) {}

  MOCK_METHOD(void, add_process, (const process::ProcessPtr& ptr), (override));
  MOCK_METHOD(void, remove_process, (const process::Process&), (override));
};

void setup_mocks(MockBackend& backend)
{
  ON_CALL(backend, max_memory()).WillByDefault(testing::Return(4096));
  ON_CALL(backend, max_vcpus()).WillByDefault(testing::Return(4));

  using callback_t =
      std::function<void(std::shared_ptr<backend::ProcessInstance>&&, std::optional<std::string>)>;

  ON_CALL(backend, allocate_process(testing::_, testing::_, testing::_))
      .WillByDefault([&](auto, const auto&, callback_t&& callback) {
        callback(std::make_shared<MockBackendInstance>(), "");
      });
}

#endif
