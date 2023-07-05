
#include <praas/common/exceptions.hpp>
#include <praas/control-plane/resources.hpp>

#include <gtest/gtest.h>

using namespace praas::control_plane;

TEST(Resources, AddApplication)
{

  Resources resources;
  std::string app_name{"app"};
  std::string second_app_name{"app2"};
  std::string third_app_name{"app3"};

  {
    Application app{app_name, ApplicationResources{}};
    EXPECT_NO_THROW(resources.add_application(std::move(app)));
  }

  {
    Resources::ROAccessor acc;
    resources.get_application(app_name, acc);
    ASSERT_EQ(acc.empty(), false);
    EXPECT_NE(acc.get(), nullptr);
    EXPECT_EQ(acc.get()->name(), app_name);
  }

  {
    Application app{second_app_name, ApplicationResources{}};
    EXPECT_NO_THROW(resources.add_application(std::move(app)));
  }

  {
    Resources::ROAccessor acc;
    resources.get_application(second_app_name, acc);
    ASSERT_EQ(acc.empty(), false);
    EXPECT_NE(acc.get(), nullptr);
    EXPECT_EQ(acc.get()->name(), second_app_name);
  }

  {
    Resources::ROAccessor acc;
    resources.get_application(third_app_name, acc);
    ASSERT_EQ(acc.empty(), true);
    EXPECT_EQ(acc.get(), nullptr);
  }

  {
    Application app;
    EXPECT_THROW(
        resources.add_application(std::move(app)), praas::common::InvalidConfigurationError
    );
  }

  {
    Application app{second_app_name, ApplicationResources{}};
    EXPECT_THROW(resources.add_application(std::move(app)), praas::common::ObjectExists);
  }
}

TEST(Resources, DeleteApplication)
{

  Resources resources;
  std::string app_name{"app"};
  std::string second_app_name{"app2"};
  std::string third_app_name{"app3"};

  {
    EXPECT_THROW(resources.delete_application(app_name), praas::common::ObjectDoesNotExist);
  }

  {
    Application app{app_name, ApplicationResources{}};
    EXPECT_NO_THROW(resources.add_application(std::move(app)));

    EXPECT_NO_THROW(resources.delete_application(app_name));
    EXPECT_THROW(resources.delete_application(app_name), praas::common::ObjectDoesNotExist);
    EXPECT_THROW(resources.delete_application(second_app_name), praas::common::ObjectDoesNotExist);
  }

  {
    Application app;
    EXPECT_THROW(resources.delete_application(""), praas::common::InvalidConfigurationError);
  }
}
