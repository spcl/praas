
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

TEST_F(IntegrationLocalInvocation, AutomaticSwap)
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
    ASSERT_TRUE(proc->is_alive());

    Result msg;
    msg.msg = "TEST_SWAP";
    auto res = get_input_binary(msg);

    auto invoc = proc->invoke("state-put", "invocation-id", res.data(), res.size());
    ASSERT_EQ(invoc.return_code, 0);
    ASSERT_TRUE(proc->is_alive());

    invoc = proc->invoke("state-get", "invocation-id", res.data(), res.size());
    ASSERT_EQ(invoc.return_code, 0);
    ASSERT_TRUE(proc->is_alive());

    // Wait for the downscaler to do its job.
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Will fail at writing
    auto invoc2 = proc->invoke("state-get", "invocation-id", res.data(), res.size());
    ASSERT_EQ(invoc2.return_code, 1);

    // Will read closure message
    ASSERT_FALSE(proc->is_alive());

    ASSERT_FALSE(proc->is_alive());

    auto new_proc = praas.swapin_process(app_name, "alloc_invoc_process");
    ASSERT_TRUE(new_proc.has_value());
    ASSERT_TRUE(new_proc->connect());
    ASSERT_TRUE(new_proc->is_alive());

    auto invoc3 = new_proc->invoke("state-get", "invocation-id", res.data(), res.size());
    ASSERT_EQ(invoc3.return_code, 0);

    ASSERT_TRUE(praas.stop_process(new_proc.value()));

    ASSERT_TRUE(praas.delete_application(app_name));
  }
}

TEST_F(IntegrationLocalInvocation, ControlPlaneAutomaticSwap)
{
  praas::common::http::HTTPClientFactory::initialize(1);
  {
    praas::sdk::PraaS praas{fmt::format("http://127.0.0.1:{}", 9000)};

    std::string app_name("test_swap");
    ASSERT_TRUE(
      praas.create_application(app_name, "spcleth/praas:proc-local")
    );

    Result msg;
    msg.msg = "TEST_SWAP";
    auto res = get_input_binary(msg);

    auto invoc = praas.invoke_async(app_name, "state-put", res);
    auto result = invoc.get();
    ASSERT_EQ(result.return_code, 0);

    // Wait for the downscaler to do its job.
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Invoke again, verify swap works
    invoc = praas.invoke_async(app_name, "state-get", res, result.process_name);
    auto new_invoc_res = invoc.get();

    ASSERT_EQ(new_invoc_res.return_code, 0);
    ASSERT_EQ(new_invoc_res.process_name, result.process_name);
  }
}
