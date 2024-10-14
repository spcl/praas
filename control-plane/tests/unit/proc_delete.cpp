
#include "../mocks.hpp"

#include <praas/common/exceptions.hpp>
#include <praas/control-plane/config.hpp>
#include <praas/control-plane/process.hpp>

#include <gtest/gtest.h>

using namespace praas::control_plane;

class DeleteProcessTest : public ::testing::Test {
protected:
  DeleteProcessTest() : poller(workers) {}

  void SetUp() override
  {
    _app_create = Application{"app", ApplicationResources{}};
    setup_mocks(backend);
  }

  Application _app_create;
  MockBackend backend;
  deployment::Local deployment{"/"};
  MockWorkers workers{backend, deployment};
  MockTCPServer poller;
};

/**
 *
 * Test of deletion - we need to create process, swap it, and then delete the swap.
 * - Delete correct swap. Verify that delete function is called.
 *   Verify the process does not exist anymore.
 * - Delete non-existing process - should fail.
 * - Delete still swapping process - should fail.
 *
 *   TODO: verify that the correct deletion calls delete_swap in the deployment!
 *
 */

TEST_F(DeleteProcessTest, DeleteCorrect)
{

  std::string proc_name{"proc1"};
  std::string resource_name{"sandbox"};
  process::Resources resources{"1", "128", resource_name};

  {
    std::promise<bool> p;
    _app_create.add_process(
        backend, poller, proc_name, std::move(resources),
        [&p](process::ProcessPtr proc, const std::optional<std::string>&) {
          p.set_value(proc != nullptr);
        },
        false
    );
    ASSERT_TRUE(p.get_future().get());

    // Manually change the process to be allocated.
    {
      auto [lock, proc] = _app_create.get_process(proc_name);
      proc->set_status(process::Status::ALLOCATED);
    }

    _app_create.swap_process(proc_name, deployment, nullptr);
    _app_create.swapped_process(proc_name, 0, 0);
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
  process::Resources resources{"1", "128", resource_name};

  {
    std::promise<bool> p;
    _app_create.add_process(
        backend, poller, proc_name, std::move(resources),
        [&p](process::ProcessPtr proc, const std::optional<std::string>&) {
          p.set_value(proc != nullptr);
        },
        false
    );
    ASSERT_TRUE(p.get_future().get());

    // Manually change the process to be allocated.
    {
      auto [lock, proc] = _app_create.get_process(proc_name);
      proc->set_status(process::Status::ALLOCATED);
    }

    _app_create.swap_process(proc_name, deployment, nullptr);
  }

  EXPECT_THROW(
      _app_create.delete_process(proc_name, deployment), praas::common::ObjectDoesNotExist
  );
}
