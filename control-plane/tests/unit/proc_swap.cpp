
#include "mocks.hpp"

#include <praas/common/exceptions.hpp>
#include <praas/control-plane/config.hpp>
#include <praas/control-plane/process.hpp>

#include <gtest/gtest.h>

class SwapProcessTest : public ::testing::Test {
protected:
  SwapProcessTest() : poller(workers) {}

  void SetUp() override
  {
    _app_create = Application{"app", ApplicationResources{}};

    setup_mocks(backend);
  }

  Application _app_create;
  MockBackend backend;
  MockDeployment deployment;
  MockWorkers workers{backend, deployment};
  MockTCPServer poller;
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

  EXPECT_THROW(_app_create.swap_process(proc_name, deployment), praas::common::InvalidProcessState);
}

TEST_F(SwapProcessTest, SwapProcessFail)
{

  std::string proc_name{"proc1"};
  std::string resource_name{"sandbox"};
  process::Resources resources{1, 128, resource_name};

  _app_create.add_process(backend, poller, proc_name, std::move(resources));

  // Incorrect swap of allocated process

  EXPECT_CALL(deployment, get_location(testing::_)).Times(0);

  EXPECT_THROW(_app_create.swap_process(proc_name, deployment), praas::common::InvalidProcessState);

  // Non-existing process

  EXPECT_CALL(deployment, get_location(testing::_)).Times(0);

  EXPECT_THROW(_app_create.swap_process("proc2", deployment), praas::common::ObjectDoesNotExist);
}

TEST_F(SwapProcessTest, FullSwapProcess)
{

  std::string proc_name{"proc2"};
  std::string resource_name{"sandbox"};
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

    EXPECT_THROW(_app_create.get_process(proc_name), praas::common::ObjectDoesNotExist);

    auto [lock, proc] = _app_create.get_swapped_process(proc_name);
    EXPECT_EQ(proc->name(), proc_name);
    EXPECT_EQ(proc->status(), process::Status::SWAPPED_OUT);
  }

  // Attempt another swap of already swapped process

  EXPECT_CALL(deployment, get_location(testing::_)).Times(0);

  EXPECT_THROW(_app_create.swap_process(proc_name, deployment), praas::common::ObjectDoesNotExist);
}
