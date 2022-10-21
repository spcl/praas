
#include <praas/common/exceptions.hpp>
#include <praas/control-plane/backend.hpp>
#include <praas/control-plane/deployment.hpp>
#include <praas/control-plane/handle.hpp>
#include <praas/control-plane/poller.hpp>
#include <praas/control-plane/process.hpp>
#include <praas/control-plane/resources.hpp>

#include <gmock/gmock-actions.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace praas::control_plane;

class MockPoller : public poller::Poller {
public:
  MOCK_METHOD(void, add_handle, (const process::ProcessHandle*), (override));
  MOCK_METHOD(void, remove_handle, (const process::ProcessHandle*), (override));
};

class MockDeployment : public deployment::Deployment {
public:
  MOCK_METHOD(std::unique_ptr<state::SwapLocation>, get_location, (std::string), (override));
};

class MockBackend : public backend::Backend {
public:
  MOCK_METHOD(void, allocate_process, (process::ProcessHandle&, const process::Resources&), ());
  MOCK_METHOD(int, max_memory, (), (const));
  MOCK_METHOD(int, max_vcpus, (), (const));
};

class SwapProcessTest : public ::testing::Test {
protected:
  void SetUp() override
  {
    _app_create = Application{"app"};

    ON_CALL(backend, max_memory()).WillByDefault(testing::Return(4096));
    ON_CALL(backend, max_vcpus()).WillByDefault(testing::Return(4));
  }

  Application _app_create;
  MockBackend backend;
  MockPoller poller;
  MockDeployment deployment;
};

/**
 *
 * Swapping tests:
 * - Swap a regular, allocated process. Should succeed, and call the swap function.
 *   Verify the correct status.
 * - Swap a process that is in the middle of swapping - should fail.
 * - Swap an allocating process - should fail.
 * - Swap a non-existing process - should fail.
 * - Simulate the call to "swapped process" by poller - verify the correct status.
 * - Swap an already swapped process - should fail.
 *
 */

TEST_F(SwapProcessTest, SwapProcess)
{

  std::string proc_name{"proc1"};
  std::string resource_name{"sandbox"};
  process::ProcessHandle handle{_app_create, backend};
  handle.instance_id = "id";
  handle.resource_id = resource_name;
  process::Resources resources{1, 128, resource_name};

  _app_create.add_process(backend, poller, proc_name, std::move(resources));

  // Manually change the process to be allocated.
  {
    auto [lock, proc] = _app_create.get_process(proc_name);
    proc->set_status(process::Status::ALLOCATED);
  }

  EXPECT_CALL(deployment, get_location(testing::_)).Times(1);

  {
    _app_create.swap_process(proc_name, deployment);
    auto [lock, proc] = _app_create.get_process(proc_name);

    EXPECT_EQ(proc->name(), proc_name);
    EXPECT_EQ(proc->status(), process::Status::SWAPPING_OUT);
  }

  // Attempt another swap

  EXPECT_CALL(deployment, get_location(testing::_)).Times(0);

  EXPECT_THROW(
    _app_create.swap_process(proc_name, deployment),
    praas::common::SwappingNotAllocatedProcess
  );
}

TEST_F(SwapProcessTest, SwapProcessFail)
{

  std::string proc_name{"proc1"};
  std::string resource_name{"sandbox"};
  process::ProcessHandle handle{_app_create, backend};
  handle.instance_id = "id";
  handle.resource_id = resource_name;
  process::Resources resources{1, 128, resource_name};

  _app_create.add_process(backend, poller, proc_name, std::move(resources));

  // Incorrect swap of allocated process

  EXPECT_CALL(deployment, get_location(testing::_)).Times(0);

  EXPECT_THROW(
    _app_create.swap_process(proc_name, deployment),
    praas::common::SwappingNotAllocatedProcess
  );

  // Non-existing process

  EXPECT_CALL(deployment, get_location(testing::_)).Times(0);

  EXPECT_THROW(
    _app_create.swap_process("proc2", deployment),
    praas::common::ObjectDoesNotExist
  );

}

TEST_F(SwapProcessTest, FullSwapProcess)
{

  std::string proc_name{"proc2"};
  std::string resource_name{"sandbox"};
  process::ProcessHandle handle{_app_create, backend};
  handle.instance_id = "id";
  handle.resource_id = resource_name;
  process::Resources resources{1, 128, resource_name};

  _app_create.add_process(backend, poller, proc_name, std::move(resources));

  // Manually change the process to be allocated.
  {
    auto [lock, proc] = _app_create.get_process(proc_name);
    proc->set_status(process::Status::ALLOCATED);
  }

  {
    EXPECT_CALL(deployment, get_location(testing::_)).Times(1);

    _app_create.swap_process(proc_name, deployment);
    auto [lock, proc] = _app_create.get_process(proc_name);

    EXPECT_EQ(proc->name(), proc_name);
    EXPECT_EQ(proc->status(), process::Status::SWAPPING_OUT);
  }

  {
    // Manually progress swapping and verify it is swapped
    _app_create.swapped_process(proc_name);

    EXPECT_THROW(
      _app_create.get_process(proc_name),
      praas::common::ObjectDoesNotExist
    );

    auto [lock, proc] = _app_create.get_swapped_process(proc_name);
    EXPECT_EQ(proc->name(), proc_name);
    EXPECT_EQ(proc->status(), process::Status::SWAPPED_OUT);
  }

  // Attempt another swap of already swapped process

  EXPECT_CALL(deployment, get_location(testing::_)).Times(0);

  EXPECT_THROW(
    _app_create.swap_process(proc_name, deployment),
    praas::common::ObjectDoesNotExist
  );
}
