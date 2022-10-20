
#include <praas/common/exceptions.hpp>
#include <praas/control-plane/config.hpp>

#include <sstream>

#include <gtest/gtest.h>

using namespace praas::control_plane::config;

TEST(Config, BasicConfig)
{
  std::string config = R"(
    {
      "verbose": true
    }
  )";

  std::stringstream stream{config};
  Config cfg = Config::deserialize(stream);

  EXPECT_EQ(cfg.verbose, true);

}
