
#include <praas/common/exceptions.hpp>
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

using namespace praas::control_plane;

class MockWorkers : public worker::Workers{
public:
  MockWorkers(backend::Backend & backend) : worker::Workers(config::Workers{}, backend, resources) {}
  Resources resources;
};

class MockTCPServer : public tcpserver::TCPServer {
public:
  MockTCPServer(MockWorkers & workers) : tcpserver::TCPServer(config::TCPServer{}, workers) {}

  MOCK_METHOD(void, add_process, (const process::ProcessPtr& ptr), (override));
  MOCK_METHOD(void, remove_process, (const process::Process &), (override));
};

class MockBackend : public backend::Backend {
public:
  MOCK_METHOD(std::shared_ptr<backend::ProcessInstance>, allocate_process, (process::ProcessPtr, const process::Resources&), ());
  MOCK_METHOD(void, shutdown, (const std::shared_ptr<backend::ProcessInstance> &), ());
  MOCK_METHOD(int, max_memory, (), (const));
  MOCK_METHOD(int, max_vcpus, (), (const));
};

class DeleteProcessTest : public ::testing::Test {
protected:

  DeleteProcessTest():
    poller(workers)
    {}

  void SetUp() override
  {
    _app_create = Application{"app"};

    ON_CALL(backend, max_memory()).WillByDefault(testing::Return(4096));
    ON_CALL(backend, max_vcpus()).WillByDefault(testing::Return(4));
  }

  Application _app_create;
  MockBackend backend;
  MockWorkers workers{backend};
  MockTCPServer poller;
  deployment::Local deployment{"/"};
};

/**
 *
 * Test of deletion - we need to create process, swap it, and then delete the swap.
 * - Delete correct swap. Verify that delete function is called.
 *   Verify the process does not exist anymore.
 * - Delete non-existing process - should fail.
 * - Delete still swapping process - should fail.
 *
 */

TEST_F(DeleteProcessTest, DeleteCorrect)
{

  std::string proc_name{"proc1"};
  std::string resource_name{"sandbox"};
  process::Resources resources{1, 128, resource_name};

  {
    _app_create.add_process(backend, poller, proc_name, std::move(resources));

    // Manually change the process to be allocated.
    {
      auto [lock, proc] = _app_create.get_process(proc_name);
      proc->set_status(process::Status::ALLOCATED);
    }

    _app_create.swap_process(proc_name, deployment);
    _app_create.swapped_process(proc_name);
  }

  _app_create.delete_process(proc_name, deployment);

  EXPECT_THROW(_app_create.get_swapped_process(proc_name), praas::common::ObjectDoesNotExist);
}

TEST_F(DeleteProcessTest, DeleteNotExisting)
{

  std::string proc_name{"proc3"};

  EXPECT_THROW(
      _app_create.delete_process(proc_name, deployment), praas::common::ObjectDoesNotExist
  );
}

TEST_F(DeleteProcessTest, DeleteWhileSwapping)
{

  std::string proc_name{"proc2"};
  std::string resource_name{"sandbox"};
  process::Resources resources{1, 128, resource_name};

  {
    _app_create.add_process(backend, poller, proc_name, std::move(resources));

    // Manually change the process to be allocated.
    {
      auto [lock, proc] = _app_create.get_process(proc_name);
      proc->set_status(process::Status::ALLOCATED);
    }

    _app_create.swap_process(proc_name, deployment);
  }

  EXPECT_THROW(
      _app_create.delete_process(proc_name, deployment), praas::common::ObjectDoesNotExist
  );
}
