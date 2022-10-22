
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
  MOCK_METHOD(void, delete_swap, (const state::SwapLocation& ), (override));
};

class MockBackend : public backend::Backend {
public:
  MOCK_METHOD(void, allocate_process, (process::ProcessHandle&, const process::Resources&), ());
  MOCK_METHOD(int, max_memory, (), (const));
  MOCK_METHOD(int, max_vcpus, (), (const));
};

class CreateProcessTest : public ::testing::Test {
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

TEST_F(CreateProcessTest, CreateProcess)
{

  // Correct allocation
  {
    std::string proc_name{"proc1"};
    std::string resource_name{"sandbox"};
    process::ProcessHandle handle{_app_create, backend};
    handle.instance_id = "id";
    handle.resource_id = resource_name;
    process::Resources resources{1, 128, resource_name};

    EXPECT_CALL(backend, allocate_process(testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillOnce([&](process::ProcessHandle& handle, const process::Resources&) -> void {
          handle.resource_id = resource_name;
          handle.instance_id = "id";
        });

    EXPECT_CALL(poller, add_handle(testing::_)).Times(1);
    EXPECT_CALL(poller, remove_handle(testing::_)).Times(0);

    // Constraints should be verified
    EXPECT_CALL(backend, max_memory()).Times(1);
    EXPECT_CALL(backend, max_vcpus()).Times(1);

    _app_create.add_process(backend, poller, proc_name, std::move(resources));
    auto [lock, proc] = _app_create.get_process(proc_name);

    EXPECT_EQ(proc->name(), proc_name);
    EXPECT_EQ(proc->status(), process::Status::ALLOCATING);
    EXPECT_EQ(&proc->handle().backend.get(), &backend);
    EXPECT_EQ(proc->handle().resource_id, resource_name);
  }

  // Duplicated name, no backend call
  {

    std::string proc_name{"proc1"};
    std::string resource_name{"sandbox"};
    process::Resources resources{1, 128, resource_name};

    EXPECT_CALL(backend, allocate_process(testing::_, testing::_)).Times(testing::Exactly(0));

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
    process::Resources resources{1, 128, resource_name};
    EXPECT_CALL(backend, allocate_process(testing::_, testing::_)).Times(testing::Exactly(0));

    EXPECT_THROW(
        _app_create.add_process(backend, poller, "", std::move(resources)),
        praas::common::InvalidConfigurationError
    );

    // Incorrect number of vCPUs

    std::string proc_name{"proc2"};
    resources = process::Resources{5, 128, resource_name};
    EXPECT_CALL(backend, allocate_process(testing::_, testing::_)).Times(testing::Exactly(0));
    EXPECT_CALL(backend, max_vcpus()).Times(1);

    EXPECT_THROW(
        _app_create.add_process(backend, poller, proc_name, std::move(resources)),
        praas::common::InvalidConfigurationError
    );
    EXPECT_THROW(_app_create.get_process(proc_name), praas::common::ObjectDoesNotExist);

    // Incorrect amount of memory

    resources = process::Resources{1, 8192, resource_name};
    EXPECT_CALL(backend, allocate_process(testing::_, testing::_)).Times(testing::Exactly(0));
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
  process::Resources resources{1, 128, resource_name};

  EXPECT_CALL(backend, allocate_process(testing::_, testing::_))
      .Times(testing::Exactly(1))
      .WillOnce(testing::Throw(praas::common::FailedAllocationError{"fail"}));

  // Call poll + remove poll
  EXPECT_CALL(poller, add_handle(testing::_)).Times(1);
  EXPECT_CALL(poller, remove_handle(testing::_)).Times(1);

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