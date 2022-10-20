
#include <praas/common/exceptions.hpp>
#include <praas/control-plane/backend.hpp>
#include <praas/control-plane/resources.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace praas::control_plane;

class MockBackend : public backend::Backend {
public:
  MOCK_METHOD(backend::ProcessHandle, allocate_process, (const process::Resources&), ());
  MOCK_METHOD(int, max_memory, (), (const));
  MOCK_METHOD(int, max_vcpus, (), (const));
};

class ProcessTest : public ::testing::Test {
protected:
  void SetUp() override
  {
    _app_create = Application{"app"};

    EXPECT_CALL(backend, max_memory()).WillRepeatedly(testing::Return(4096));
    EXPECT_CALL(backend, max_vcpus()).WillRepeatedly(testing::Return(4));
  }

  Application _app_create;
  MockBackend backend;
};

TEST_F(ProcessTest, CreateProcess)
{

  // Correct allocation
  {
    std::string proc_name{"proc1"};
    std::string resource_name{"sandbox"};
    backend::ProcessHandle handle{backend, "id", resource_name};
    process::Resources resources{1, 128, resource_name};

    EXPECT_CALL(backend, allocate_process(testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(handle));

    _app_create.add_process(backend, proc_name, std::move(resources));
    auto [lock, proc] = _app_create.get_process(proc_name);

    EXPECT_EQ(proc->name(), proc_name);
    EXPECT_EQ(proc->status(), process::Status::ALLOCATED);
    ASSERT_TRUE(proc->has_handle());
    EXPECT_EQ(&proc->handle().backend.get(), &backend);
    EXPECT_EQ(proc->handle().resource_id, resource_name);
  }

  // Duplicated name, no backend call
  {

    std::string proc_name{"proc1"};
    std::string resource_name{"sandbox"};
    process::Resources resources{1, 128, resource_name};

    EXPECT_CALL(backend, allocate_process(testing::_)).Times(testing::Exactly(0));

    EXPECT_THROW(
        _app_create.add_process(backend, proc_name, std::move(resources)),
        praas::common::ObjectExists
    );
  }
}

TEST_F(ProcessTest, CreateProcessIncorrectConfig)
{
  // Incorrect config, no call
  {

    // Empty process name

    std::string resource_name{"sandbox"};
    process::Resources resources{1, 128, resource_name};
    EXPECT_CALL(backend, allocate_process(testing::_)).Times(testing::Exactly(0));

    EXPECT_THROW(
        _app_create.add_process(backend, "", std::move(resources)),
        praas::common::InvalidConfigurationError
    );

    // Incorrect number of vCPUs

    std::string proc_name{"proc2"};
    resources = process::Resources{5, 128, resource_name};
    EXPECT_CALL(backend, allocate_process(testing::_)).Times(testing::Exactly(0));

    EXPECT_THROW(
        _app_create.add_process(backend, proc_name, std::move(resources)),
        praas::common::InvalidConfigurationError
    );
    EXPECT_THROW(_app_create.get_process(proc_name), praas::common::ObjectDoesNotExist);

    // Incorrect amount of memory

    resources = process::Resources{1, 8192, resource_name};
    EXPECT_CALL(backend, allocate_process(testing::_)).Times(testing::Exactly(0));

    EXPECT_THROW(
        _app_create.add_process(backend, proc_name, std::move(resources)),
        praas::common::InvalidConfigurationError
    );
    EXPECT_THROW(_app_create.get_process(proc_name), praas::common::ObjectDoesNotExist);
  }
}

TEST_F(ProcessTest, CreateProcessFailure)
{

  std::string proc_name{"proc3"};
  std::string resource_name{"sandbox"};
  backend::ProcessHandle handle{backend, "id", resource_name};
  process::Resources resources{1, 128, resource_name};

  EXPECT_CALL(backend, allocate_process(testing::_))
      .Times(testing::Exactly(1))
      .WillOnce(testing::Throw(praas::common::FailedAllocationError{"fail"}));

  EXPECT_THROW(
    _app_create.add_process(backend, proc_name, std::move(resources)),
    praas::common::FailedAllocationError
  );
  // Verify that there is no leftover left
  EXPECT_THROW(_app_create.get_process(proc_name), praas::common::ObjectDoesNotExist);

}
