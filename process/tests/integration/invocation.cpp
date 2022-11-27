
#include <praas/common/exceptions.hpp>
#include <praas/common/messages.hpp>
#include <praas/process/controller/config.hpp>
#include <praas/process/controller/controller.hpp>
#include <praas/process/controller/remote.hpp>
#include <praas/process/runtime/ipc/messages.hpp>

#include "examples/cpp/test.hpp"

#include <boost/iostreams/device/array.hpp>
#include <future>
#include <thread>

#include <boost/interprocess/streams/bufferstream.hpp>
#include <boost/iostreams/stream.hpp>
#include <cereal/archives/binary.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace praas::process;

class MockTCPServer : public remote::Server {
public:
  MockTCPServer() = default;

  MOCK_METHOD(void, poll, (), (override));
  MOCK_METHOD(void, put_message, (), (override));
  MOCK_METHOD(
      void, invocation_result,
      (const std::optional<std::string>& remote_process, std::string_view invocation_id,
       runtime::Buffer<char> payload),
      (override)
  );
};

size_t generate_input(int arg1, int arg2, runtime::Buffer<char> buf)
{
  Input input{arg1, arg2};
  boost::interprocess::bufferstream stream(buf.val, buf.size);
  cereal::BinaryOutputArchive archive_out{stream};
  archive_out(input);
  assert(stream.good());
  size_t pos = stream.tellp();
  return pos;
}

int get_output(runtime::Buffer<char> buf)
{
  Output out;
  boost::iostreams::stream<boost::iostreams::array_source> stream(buf.val, buf.size);
  cereal::BinaryInputArchive archive_in{stream};
  out.load(archive_in);

  return out.result;
}

/**
 * Regular invocations.
 * Return payload 0.
 * High return payload (megabytes), requiring multi-part messages).
 * Error code from failing invocations.
 * Python invocations.
 */

TEST(ProcessInvocationTest, SimpleInvocation)
{
  const int BUF_LEN = 1024;
  std::string function_name = "test";
  std::array<std::string, 2> invocation_id = { "first_id", "second_id" };

  std::array<std::tuple<int, int>, 2> args = { std::make_tuple(42, 4), std::make_tuple(-1, 35) };
  std::array<int, 2> results = { 46, 34 };

  config::Controller cfg;
  cfg.set_defaults();

  cfg.verbose = true;
  // FIXME: compiler time defaults
  cfg.code.location = "/work/serverless/2022/praas/code/praas/process/tests/integration";
  cfg.code.config_location = "configuration.json";

  Controller controller{cfg};
  MockTCPServer server;
  controller.set_remote(&server);

  std::thread controller_thread{&Controller::start, &controller};

  runtime::BufferQueue<char> buffers(10, 1024);

  // First invocation
  {
    int idx = 0;
    praas::common::message::InvocationRequest msg;
    msg.function_name(function_name);
    msg.invocation_id(invocation_id[idx]);

    auto buf = buffers.retrieve_buffer(BUF_LEN);
    buf.len = generate_input(std::get<0>(args[idx]), std::get<1>(args[idx]), buf);
    // Send more data than needed - check that it still works
    msg.payload_size(buf.len + 64);

    std::promise<void> finished;
    std::optional<std::string> process;
    std::string id;
    runtime::Buffer<char> payload;
    EXPECT_CALL(server, invocation_result)
        .WillOnce(testing::DoAll(
            testing::SaveArg<0>(&process), testing::SaveArg<1>(&id),
            testing::SaveArg<2>(&payload), testing::Invoke([&finished]() { finished.set_value(); })
        ));

    controller.dataplane_message(std::move(msg), std::move(buf));

    // Wait for the invocation to finish
    ASSERT_EQ(std::future_status::ready, finished.get_future().wait_for(std::chrono::seconds(3)));

    // Dataplane message
    EXPECT_FALSE(process.has_value());
    EXPECT_EQ(id, invocation_id[idx]);

    ASSERT_TRUE(payload.len > 0);
    int res = get_output(payload);
    EXPECT_EQ(res, results[idx]);
  }

  // Second invocation
  {
    int idx = 1;
    praas::common::message::InvocationRequest msg;
    msg.function_name(function_name);
    msg.invocation_id(invocation_id[idx]);

    auto buf = buffers.retrieve_buffer(BUF_LEN);
    buf.len = generate_input(std::get<0>(args[idx]), std::get<1>(args[idx]), buf);
    // Send more data than needed - check that it still works
    msg.payload_size(buf.len + 64);

    std::promise<void> finished;
    std::optional<std::string> process;
    std::string id;
    runtime::Buffer<char> payload;
    EXPECT_CALL(server, invocation_result)
        .WillOnce(testing::DoAll(
            testing::SaveArg<0>(&process), testing::SaveArg<1>(&id),
            testing::SaveArg<2>(&payload), testing::Invoke([&finished]() { finished.set_value(); })
        ));

    controller.dataplane_message(std::move(msg), std::move(buf));

    // Wait for the invocation to finish
    ASSERT_EQ(std::future_status::ready, finished.get_future().wait_for(std::chrono::seconds(3)));

    // Dataplane message
    EXPECT_FALSE(process.has_value());
    EXPECT_EQ(id, invocation_id[idx]);

    ASSERT_TRUE(payload.len > 0);
    int res = get_output(payload);
    EXPECT_EQ(res, results[idx]);
  }

  controller.shutdown();
  controller_thread.join();
}
