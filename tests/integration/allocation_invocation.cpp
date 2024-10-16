
#include <praas/common/http.hpp>
#include <praas/control-plane/config.hpp>
#include <praas/control-plane/server.hpp>
#include <praas/sdk/praas.hpp>

#include <chrono>
#include <thread>

#include <boost/iostreams/stream.hpp>
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

std::string get_output_binary(const char* buffer, size_t size)
{
  Result out;
  boost::iostreams::stream<boost::iostreams::array_source> stream(buffer, size);
  cereal::BinaryInputArchive archive_in{stream};
  out.load(archive_in);

  return out.msg;
}

TEST_F(IntegrationLocalInvocation, AllocationInvoke)
{
  praas::common::http::HTTPClientFactory::initialize(1);
  {
    praas::sdk::PraaS praas{fmt::format("http://127.0.0.1:{}", 9000)};

    std::string app_name("test_alloc_invoc");
    ASSERT_TRUE(
        praas.create_application(app_name, "spcleth/praas-examples:hello-world-cpp")
    );

    auto proc = praas.create_process(app_name, "alloc_invoc_process", "1", "1024");
    ASSERT_TRUE(proc.has_value());

    ASSERT_TRUE(proc->connect());

    auto invoc = proc->invoke("hello-world", "invocation-id", nullptr, 0);

    ASSERT_EQ(invoc.return_code, 0);
    EXPECT_EQ("Hello, world!", get_output_binary(invoc.payload.get(), invoc.payload_len));

    ASSERT_TRUE(praas.stop_process(proc.value()));

    ASSERT_TRUE(praas.delete_application(app_name));
  }
}
