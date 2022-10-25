#include <praas/common/uuid.hpp>

#include <gtest/gtest.h>

using namespace praas::common;

TEST(UUID, UUIDStrConversion)
{
  auto str = "47183823-2574-4bfd-b411-99ed177d3e43";
  auto id = uuids::uuid::from_string(str);
  ASSERT_TRUE(id.has_value());

  std::string uuid_str = UUID::str(std::span(id.value().as_bytes()));

  EXPECT_EQ(str, uuid_str);
}
