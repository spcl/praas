
#include "../mocks.hpp"

#include <praas/common/exceptions.hpp>
#include <praas/control-plane/config.hpp>
#include <praas/control-plane/process.hpp>

#include <gtest/gtest.h>

class CreateProcessTest : public ::testing::Test {
protected:
  CreateProcessTest() : poller(workers) {}

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

TEST_F(CreateProcessTest, CreateProcess)
{
  // Correct allocation
  {
    std::string proc_name{"proc1"};
    std::string resource_name{"sandbox"};
    process::Resources resources{"1", "128", resource_name};

    EXPECT_CALL(backend, allocate_process(testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(1));

    EXPECT_CALL(poller, add_process(testing::_)).Times(1);
    EXPECT_CALL(poller, remove_process(testing::_)).Times(0);

    // Constraints should be verified
    EXPECT_CALL(backend, max_memory()).Times(1);
    EXPECT_CALL(backend, max_vcpus()).Times(1);

    _app_create.add_process(backend, poller, proc_name, std::move(resources), false);
    auto [lock, proc] = _app_create.get_process(proc_name);

    EXPECT_EQ(proc->name(), proc_name);
    EXPECT_EQ(proc->status(), process::Status::ALLOCATING);
  }

  // Duplicated name, no backend call
  {

    std::string proc_name{"proc1"};
    std::string resource_name{"sandbox"};
    process::Resources resources{"1", "128", resource_name};

    EXPECT_CALL(backend, allocate_process(testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(0));

    // Constraints should be verified
    EXPECT_CALL(backend, max_memory()).Times(1);
    EXPECT_CALL(backend, max_vcpus()).Times(1);

    EXPECT_THROW(
        _app_create.add_process(backend, poller, proc_name, std::move(resources)),
        praas::common::ObjectExists
    );
  }
}

TEST_F(CreateProcessTest, CreateProcessIncorrectConfig)
{
  // Incorrect config, no call
  {

    // Empty process name

    std::string resource_name{"sandbox"};
    process::Resources resources{"1", "128", resource_name};
    EXPECT_CALL(backend, allocate_process(testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(0));

    EXPECT_THROW(
        _app_create.add_process(backend, poller, "", std::move(resources)),
        praas::common::InvalidConfigurationError
    );

    // Incorrect number of vCPUs

    std::string proc_name{"proc2"};
    resources = process::Resources{"5", "128", resource_name};
    EXPECT_CALL(backend, allocate_process(testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(0));
    EXPECT_CALL(backend, max_vcpus()).Times(1);

    EXPECT_THROW(
        _app_create.add_process(backend, poller, proc_name, std::move(resources)),
        praas::common::InvalidConfigurationError
    );
    EXPECT_THROW(_app_create.get_process(proc_name), praas::common::ObjectDoesNotExist);

    // Incorrect amount of memory

    resources = process::Resources{"1", "8192", resource_name};
    EXPECT_CALL(backend, allocate_process(testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(0));
    EXPECT_CALL(backend, max_memory()).Times(1);

    EXPECT_THROW(
        _app_create.add_process(backend, poller, proc_name, std::move(resources)),
        praas::common::InvalidConfigurationError
    );
    EXPECT_THROW(_app_create.get_process(proc_name), praas::common::ObjectDoesNotExist);
  }
}

TEST_F(CreateProcessTest, CreateProcessFailure)
{

  std::string proc_name{"proc3"};
  std::string resource_name{"sandbox"};
  process::Resources resources{"1", "128", resource_name};

  using callback_t =
      std::function<void(std::shared_ptr<backend::ProcessInstance>&&, std::optional<std::string>)>;

  EXPECT_CALL(backend, allocate_process(testing::_, testing::_, testing::_, testing::_))
      .WillOnce([&](auto, auto, const auto&, callback_t&& callback) {
        callback(nullptr, "Failed allocation");
      });

  // Call poll + remove poll
  EXPECT_CALL(poller, add_process(testing::_)).Times(1);
  EXPECT_CALL(poller, remove_process(testing::_)).Times(1);

  // Constraints should be verified
  EXPECT_CALL(backend, max_memory()).Times(1);
  EXPECT_CALL(backend, max_vcpus()).Times(1);

  EXPECT_THROW(
      _app_create.add_process(backend, poller, proc_name, std::move(resources)),
      praas::common::FailedAllocationError
  );
  // Verify that there is no leftover left
  EXPECT_THROW(_app_create.get_process(proc_name), praas::common::ObjectDoesNotExist);
}
