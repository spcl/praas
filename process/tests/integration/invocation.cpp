
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
      (const std::optional<std::string>&, std::string_view, int, runtime::Buffer<char>),
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

class ProcessInvocationTest : public ::testing::Test {
public:
  void SetUp() override
  {
    cfg.set_defaults();
    cfg.verbose = true;
    // FIXME: compiler time defaults
    cfg.code.location = "/work/serverless/2022/praas/code/praas/process/tests/integration";
    cfg.code.config_location = "configuration.json";

    controller = std::make_unique<Controller>(cfg);
    controller->set_remote(&server);

    controller_thread = std::thread{&Controller::start, controller.get()};
  }

  void TearDown() override
  {
    controller->shutdown();
    controller_thread.join();
  }

  std::thread controller_thread;
  config::Controller cfg;
  std::unique_ptr<Controller> controller;
  MockTCPServer server;
};

/**
 * Regular invocations.
 * Return payload 0.
 * Remote message vs dataplane message.
 * High return payload (megabytes), requiring multi-part messages).
 * Error code from failing invocations.
 *
 * Python invocations.
 *
 * Multi-function invocations - 2 functions on 1 worker, 2 functions on 2 workers.
 * Verify they are being executed concurrently.
 */

TEST_F(ProcessInvocationTest, SimpleInvocation)
{
  const int BUF_LEN = 1024;
  std::string function_name = "add";
  std::string process_id = "remote-process-1";
  std::array<std::string, 2> invocation_id = { "first_id", "second_id" };

  std::array<std::tuple<int, int>, 2> args = { std::make_tuple(42, 4), std::make_tuple(-1, 35) };
  std::array<int, 2> results = { 46, 34 };

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
    int return_code;
    runtime::Buffer<char> payload;
    EXPECT_CALL(server, invocation_result)
        .WillOnce(testing::DoAll(
            testing::SaveArg<0>(&process), testing::SaveArg<1>(&id),
            testing::SaveArg<2>(&return_code),
            testing::SaveArg<3>(&payload), testing::Invoke([&finished]() { finished.set_value(); })
        ));

    controller->dataplane_message(std::move(msg), std::move(buf));

    // Wait for the invocation to finish
    ASSERT_EQ(std::future_status::ready, finished.get_future().wait_for(std::chrono::seconds(3)));

    // Dataplane message
    EXPECT_FALSE(process.has_value());
    EXPECT_EQ(id, invocation_id[idx]);
    EXPECT_EQ(return_code, 0);

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
    msg.payload_size(buf.len);

    std::promise<void> finished;
    std::optional<std::string> process;
    std::string id;
    int return_code;
    runtime::Buffer<char> payload;
    EXPECT_CALL(server, invocation_result)
        .WillOnce(testing::DoAll(
            testing::SaveArg<0>(&process), testing::SaveArg<1>(&id),
            testing::SaveArg<2>(&return_code),
            testing::SaveArg<3>(&payload), testing::Invoke([&finished]() { finished.set_value(); })
        ));

    controller->remote_message(std::move(msg), std::move(buf), process_id);

    // Wait for the invocation to finish
    ASSERT_EQ(std::future_status::ready, finished.get_future().wait_for(std::chrono::seconds(3)));

    // Remote message
    EXPECT_TRUE(process.has_value());
    EXPECT_EQ(process.value(), process_id);
    EXPECT_EQ(id, invocation_id[idx]);
    EXPECT_EQ(return_code, 0);

    ASSERT_TRUE(payload.len > 0);
    int res = get_output(payload);
    EXPECT_EQ(res, results[idx]);
  }
}

TEST_F(ProcessInvocationTest, ZeroPayloadOutput)
{
  std::string function_name = "zero_return";
  std::string invocation_id = "first_id";

  praas::common::message::InvocationRequest msg;
  msg.function_name(function_name);
  msg.invocation_id(invocation_id);

  auto buf = runtime::Buffer<char>{};

  std::promise<void> finished;
  std::optional<std::string> process;
  std::string id;
  int return_code;
  runtime::Buffer<char> payload;
  EXPECT_CALL(server, invocation_result)
      .WillOnce(testing::DoAll(
          testing::SaveArg<0>(&process), testing::SaveArg<1>(&id),
          testing::SaveArg<2>(&return_code),
          testing::SaveArg<3>(&payload), testing::Invoke([&finished]() { finished.set_value(); })
      ));

  controller->dataplane_message(std::move(msg), std::move(buf));

  // Wait for the invocation to finish
  ASSERT_EQ(std::future_status::ready, finished.get_future().wait_for(std::chrono::seconds(3)));

  // Dataplane message
  EXPECT_FALSE(process.has_value());
  EXPECT_EQ(id, invocation_id);
  EXPECT_EQ(return_code, 0);

  EXPECT_EQ(payload.len, 0);
}

TEST_F(ProcessInvocationTest, ReturnError)
{
  std::string function_name = "error_function";
  std::string invocation_id = "first_id";

  praas::common::message::InvocationRequest msg;
  msg.function_name(function_name);
  msg.invocation_id(invocation_id);

  auto buf = runtime::Buffer<char>{};

  std::promise<void> finished;
  std::optional<std::string> process;
  std::string id;
  int return_code;
  runtime::Buffer<char> payload;
  EXPECT_CALL(server, invocation_result)
      .WillOnce(testing::DoAll(
          testing::SaveArg<0>(&process), testing::SaveArg<1>(&id),
          testing::SaveArg<2>(&return_code),
          testing::SaveArg<3>(&payload), testing::Invoke([&finished]() { finished.set_value(); })
      ));

  controller->dataplane_message(std::move(msg), std::move(buf));

  // Wait for the invocation to finish
  ASSERT_EQ(std::future_status::ready, finished.get_future().wait_for(std::chrono::seconds(3)));

  // Dataplane message
  EXPECT_FALSE(process.has_value());
  EXPECT_EQ(id, invocation_id);
  EXPECT_EQ(payload.len, 0);
  EXPECT_EQ(return_code, 1);
}

TEST_F(ProcessInvocationTest, LargePayload)
{
  // 4 megabyte input - 1 mega of integers
  const int BUF_LEN = 1024 * 1024 * sizeof(int);
  std::string function_name = "large_payload";
  std::string process_id = "remote-process-1";
  std::string invocation_id = "first_id";

  runtime::BufferQueue<char> buffers(1, BUF_LEN);

  praas::common::message::InvocationRequest msg;
  msg.function_name(function_name);
  msg.invocation_id(invocation_id);

  auto buf = buffers.retrieve_buffer(BUF_LEN);
  int data_len = BUF_LEN / sizeof(int);
  int* data_input = reinterpret_cast<int*>(buf.val);
  for(int i = 0; i < data_len; ++i) {
    data_input[i] = i;
  }
  buf.len = BUF_LEN;

  msg.payload_size(buf.len);

  std::promise<void> finished;
  std::optional<std::string> process;
  std::string id;
  int return_code;
  runtime::Buffer<char> payload;
  EXPECT_CALL(server, invocation_result)
      .WillOnce(testing::DoAll(
          testing::SaveArg<0>(&process), testing::SaveArg<1>(&id),
          testing::SaveArg<2>(&return_code),
          testing::SaveArg<3>(&payload), testing::Invoke([&finished]() { finished.set_value(); })
      ));

  controller->remote_message(std::move(msg), std::move(buf), process_id);

  // Wait for the invocation to finish
  ASSERT_EQ(std::future_status::ready, finished.get_future().wait_for(std::chrono::seconds(3)));

  EXPECT_TRUE(process.has_value());
  EXPECT_EQ(process.value(), process_id);
  EXPECT_EQ(id, invocation_id);
  EXPECT_EQ(return_code, 0);

  ASSERT_TRUE(payload.len == BUF_LEN);
  int* data_output = reinterpret_cast<int*>(payload.val);
  for(int i = 0; i < data_len; ++i) {
    EXPECT_EQ(data_input[i] + 2, data_output[i]);
  }

}

TEST_F(ProcessInvocationTest, SubsequentInvocations)
{
  const int COUNT = 4;
  const int BUF_LEN = 1024;
  std::string function_name = "add";
  std::string process_id = "remote-process-1";
  std::array<std::string, COUNT> invocation_id = {
    "first_id",
    "second_id",
    "third_id",
    "fourth_id"
  };

  std::array<std::tuple<int, int>, COUNT> args = {
    std::make_tuple(42, 4), std::make_tuple(-1, 35),
    std::make_tuple(1000, 0), std::make_tuple(-33, 39)
  };
  std::array<int, COUNT> results = { 46, 34, 1000, 6 };

  using timepoint_t = std::chrono::time_point<std::chrono::system_clock>;

  std::array<std::promise<void>, COUNT> finished;
  std::array<std::optional<std::string>, COUNT> process;
  std::array<std::string, COUNT> id;
  std::array<int, COUNT> return_code;
  std::array<runtime::Buffer<char>, COUNT> payload;
  std::array<timepoint_t, COUNT> timestamps;

  runtime::BufferQueue<char> buffers(10, 1024);

  // FIFO order - we cannot do it in a loop :-(
  EXPECT_CALL(server, invocation_result)
      .WillOnce(testing::DoAll(
          testing::SaveArg<0>(&process[0]), testing::SaveArg<1>(&id[0]),
          testing::SaveArg<2>(&return_code[0]),
          testing::SaveArg<3>(&payload[0]),
          testing::Invoke([&timestamps, &finished]() {
            timestamps[0] = std::chrono::system_clock::now();
            finished[0].set_value();
          })
      ))
      .WillOnce(testing::DoAll(
          testing::SaveArg<0>(&process[1]), testing::SaveArg<1>(&id[1]),
          testing::SaveArg<2>(&return_code[1]),
          testing::SaveArg<3>(&payload[1]),
          testing::Invoke([ &timestamps, &finished]() {
            timestamps[1] = std::chrono::system_clock::now();
            finished[1].set_value();
          })
      ))
      .WillOnce(testing::DoAll(
          testing::SaveArg<0>(&process[2]), testing::SaveArg<1>(&id[2]),
          testing::SaveArg<2>(&return_code[2]),
          testing::SaveArg<3>(&payload[2]),
          testing::Invoke([ &timestamps, &finished]() {
            timestamps[2] = std::chrono::system_clock::now();
            finished[2].set_value();
          })
      ))
      .WillOnce(testing::DoAll(
          testing::SaveArg<0>(&process[3]), testing::SaveArg<1>(&id[3]),
          testing::SaveArg<2>(&return_code[3]),
          testing::SaveArg<3>(&payload[3]),
          testing::Invoke([ &timestamps, &finished]() {
            timestamps[3] = std::chrono::system_clock::now();
            finished[3].set_value();
          })
      ));

  // Submit
  for(int idx = 0; idx < COUNT; ++idx) {

    praas::common::message::InvocationRequest msg;
    msg.function_name(function_name);
    msg.invocation_id(invocation_id[idx]);

    auto buf = buffers.retrieve_buffer(BUF_LEN);
    buf.len = generate_input(std::get<0>(args[idx]), std::get<1>(args[idx]), buf);
    // Send more data than needed - check that it still works
    msg.payload_size(buf.len + 64);

    controller->dataplane_message(std::move(msg), std::move(buf));
  }

  // wait
  for(int idx = 0; idx < COUNT; ++idx) {
    ASSERT_EQ(std::future_status::ready, finished[idx].get_future().wait_for(std::chrono::seconds(3)));
  }

  // Validate result
  for(int idx = 0; idx < COUNT; ++idx) {

    // Dataplane message
    EXPECT_FALSE(process[idx].has_value());
    EXPECT_EQ(id[idx], invocation_id[idx]);
    EXPECT_EQ(return_code[idx], 0);

    ASSERT_TRUE(payload[idx].len > 0);
    int res = get_output(payload[idx]);
    EXPECT_EQ(res, results[idx]);

  }

  // Validate order
  for(int idx = 1; idx < COUNT; ++idx) {
    ASSERT_TRUE(timestamps[idx-1] < timestamps[idx]);
  }

}

