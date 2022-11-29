
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
      (const std::optional<std::string>&, std::string_view, int, runtime::Buffer<char> &&),
      (override)
  );
};

size_t generate_input(int arg1, int arg2, const runtime::Buffer<char> & buf)
{
  Input input{arg1, arg2};
  boost::interprocess::bufferstream stream(buf.data(), buf.size);
  cereal::BinaryOutputArchive archive_out{stream};
  archive_out(input);
  assert(stream.good());
  size_t pos = stream.tellp();
  return pos;
}

int get_output(const runtime::Buffer<char> & buf)
{
  Output out;
  boost::iostreams::stream<boost::iostreams::array_source> stream(buf.data(), buf.size);
  cereal::BinaryInputArchive archive_in{stream};
  out.load(archive_in);

  return out.result;
}

class ProcessMessagingTest : public testing::TestWithParam<const char*> {
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

    EXPECT_CALL(server, invocation_result)
        .WillRepeatedly(
            [&](auto _process, auto _id, int _return_code, auto && _payload) {
              process = _process;
              id = _id;
              return_code = _return_code;
              payload = std::move(_payload);
              finished.set_value();
            }
        );
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

  // Results
  std::promise<void> finished;
  std::optional<std::string> process;
  std::string id;
  int return_code;
  runtime::Buffer<char> payload;

  void reset()
  {
    process = std::nullopt;
    id.clear();
    return_code = -1;
    payload = runtime::Buffer<char>{};
    finished = std::promise<void>{};
  }
};

/**
 * Put message, get from SELF.
 *
 * Put message, get from ANY.
 *
 * Put message, get from my own name.
 *
 * Get message, returns error code (message unknown) - currently not supported.
 *
 * Put message with the same name twice - error, currently not supported.
 */

TEST_P(ProcessMessagingTest, GetPutOneWorker)
{
  const int BUF_LEN = 1024;
  std::string put_function_name = "send_message";
  std::string get_function_name = GetParam();
  std::string process_id = "remote-process-1";
  std::array<std::string, 2> invocation_id = { "first_id", "second_id" };

  runtime::BufferQueue<char> buffers(10, 1024);

  reset();

  // First invocation
  {
    int idx = 0;
    praas::common::message::InvocationRequest msg;
    msg.function_name(put_function_name);
    msg.invocation_id(invocation_id[idx]);

    auto buf = buffers.retrieve_buffer(0);

    controller->dataplane_message(std::move(msg), std::move(buf));

    // Wait for the invocation to finish
    ASSERT_EQ(std::future_status::ready, finished.get_future().wait_for(std::chrono::seconds(3)));

    // Dataplane message
    EXPECT_FALSE(process.has_value());
    EXPECT_EQ(id, invocation_id[idx]);
    EXPECT_EQ(return_code, 0);
  }

  reset();

  // Second invocation
  {
    int idx = 1;
    praas::common::message::InvocationRequest msg;
    msg.function_name(get_function_name);
    msg.invocation_id(invocation_id[idx]);

    auto buf = buffers.retrieve_buffer(0);

    controller->remote_message(std::move(msg), std::move(buf), process_id);

    // Wait for the invocation to finish
    ASSERT_EQ(std::future_status::ready, finished.get_future().wait_for(std::chrono::seconds(3)));

    // Remote message
    EXPECT_TRUE(process.has_value());
    EXPECT_EQ(process.value(), process_id);
    EXPECT_EQ(id, invocation_id[idx]);
    EXPECT_EQ(return_code, 0);
  }
}

INSTANTIATE_TEST_SUITE_P(ProcessGetPutTestSelf,
                         ProcessMessagingTest,
                         testing::Values("get_message_self", "get_message_any", "get_message_explicit")
                         );
