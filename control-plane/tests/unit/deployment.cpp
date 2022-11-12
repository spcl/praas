
#include <praas/common/exceptions.hpp>
#include <praas/control-plane/deployment.hpp>

#include <filesystem>

#include <gmock/gmock-actions.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace praas::control_plane;


TEST(DeploymentTest, LocalLocation)
{

  {
    std::filesystem::path root_path{"/"};
    std::string proc_name{"proc"};
    deployment::Local deployment{root_path};

    auto loc = deployment.get_location(proc_name);
    ASSERT_NE(loc.get(), nullptr);

    state::DiskSwapLocation* ptr = dynamic_cast<state::DiskSwapLocation*>(loc.get());
    ASSERT_NE(ptr, nullptr);
    ASSERT_EQ(ptr->path(proc_name), root_path / "swaps" / proc_name);
    ASSERT_EQ(ptr->root_path(), root_path);
  }

  {
    std::filesystem::path root_path{"/path/to/data/nested/"};
    std::string proc_name{"proc2"};
    deployment::Local deployment{root_path};

    auto loc = deployment.get_location(proc_name);
    ASSERT_NE(loc.get(), nullptr);

    state::DiskSwapLocation* ptr = dynamic_cast<state::DiskSwapLocation*>(loc.get());
    ASSERT_NE(ptr, nullptr);
    ASSERT_EQ(ptr->path(proc_name), root_path / "swaps" / proc_name);
    ASSERT_EQ(ptr->root_path(), root_path);
  }

}

