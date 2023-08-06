
#include <praas/common/http.hpp>
#include <praas/control-plane/config.hpp>
#include <praas/control-plane/server.hpp>
#include <praas/sdk/praas.hpp>

#include <chrono>
#include <thread>

#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

struct Result {
  std::string msg;

  template <typename Ar>
  void save(Ar& archive) const
  {
    archive(CEREAL_NVP(msg));
  }

  template <typename Ar>
  void load(Ar& archive)
  {
    archive(CEREAL_NVP(msg));
  }
};

class IntegrationLocalInvocation : public ::testing::Test {};

std::string get_output_binary(const std::string& response)
{
  Result out;
  std::stringstream stream{response};
  cereal::BinaryInputArchive archive_in{stream};
  out.load(archive_in);

  return out.msg;
}

TEST_F(IntegrationLocalInvocation, Invoke)
{
  praas::common::http::HTTPClientFactory::initialize(1);
  {
    praas::sdk::PraaS praas{fmt::format("http://127.0.0.1:{}", 8000)};

    ASSERT_TRUE(praas.create_application("test_invoc", "spcleth/praas-examples:hello-world-cpp"));

    auto invoc = praas.invoke("test_invoc", "hello-world", "");

    ASSERT_EQ(invoc.return_code, 0);
    EXPECT_EQ("Hello, world!", get_output_binary(invoc.response));
  }
}
