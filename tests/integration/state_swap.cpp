
#include <praas/common/http.hpp>
#include <praas/control-plane/config.hpp>
#include <praas/control-plane/server.hpp>
#include <praas/sdk/praas.hpp>

#include <boost/interprocess/streams/bufferstream.hpp>
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

std::string get_input_binary(Result& msg)
{
  std::string result;
  result.resize(1024);

  boost::interprocess::bufferstream out_stream(reinterpret_cast<char*>(result.data()), 1024);
  cereal::BinaryOutputArchive archive_out{out_stream};
  archive_out(msg);

  return result;
}

TEST_F(IntegrationLocalInvocation, AllocationInvoke)
{
  praas::common::http::HTTPClientFactory::initialize(1);
  {
    praas::sdk::PraaS praas{fmt::format("http://127.0.0.1:{}", 9000)};

    std::string app_name("test_swap");
    ASSERT_TRUE(
      praas.create_application(app_name, "spcleth/praas:proc-local")
    );

    auto proc = praas.create_process(app_name, "alloc_invoc_process", "1", "1024");
    ASSERT_TRUE(proc.has_value());

    ASSERT_TRUE(proc->connect());

    Result msg;
    msg.msg = "TEST_SWAP";
    auto res = get_input_binary(msg);

    auto invoc = proc->invoke("state-put", "invocation-id", res.data(), res.size());
    ASSERT_EQ(invoc.return_code, 0);

    auto swap_res = praas.swap_process(proc.value());
    ASSERT_TRUE(swap_res.has_value());

    auto new_proc = praas.swapin_process(app_name, "alloc_invoc_process");
    ASSERT_TRUE(new_proc.has_value());

    ASSERT_TRUE(new_proc->connect());
    invoc = new_proc->invoke("state-get", "invocation-id2", res.data(), res.size());
    ASSERT_EQ(invoc.return_code, 0);

    auto res2 = get_output_binary(invoc.payload.get(), invoc.payload_len);
    ASSERT_EQ(msg.msg, res2);

    swap_res = praas.swap_process(new_proc.value());
    ASSERT_TRUE(swap_res.has_value());

    ASSERT_TRUE(praas.delete_process(new_proc.value()));

    ASSERT_TRUE(praas.delete_application(app_name));
  }
}
