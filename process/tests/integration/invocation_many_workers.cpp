
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
      (const std::optional<std::string>&, std::string_view, int, runtime::Buffer<char>), (override)
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

class ProcessManyWorkersInvocationTest : public ::testing::Test {
public:
  void SetUp() override
  {
    cfg.set_defaults();
    cfg.verbose = true;
    // FIXME: compiler time defaults
    cfg.code.location = "/work/serverless/2022/praas/code/praas/process/tests/integration";
    cfg.code.config_location = "configuration.json";

    cfg.function_workers = 2;

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

TEST_F(ProcessManyWorkersInvocationTest, ConcurrentInvocations)
{
  const int COUNT = 4;
  const int BUF_LEN = 1024;
  std::string function_name = "add";
  std::string process_id = "remote-process-1";
  std::array<std::string, COUNT> invocation_id = {"1_id", "2_id", "3_id", "4_id"};

  std::array<std::tuple<int, int>, COUNT> args = {
      std::make_tuple(42, 4), std::make_tuple(-1, 35), std::make_tuple(1000, 0),
      std::make_tuple(-33, 39)};
  std::array<int, COUNT> results = {46, 34, 1000, 6};

  struct Result {
    std::promise<void> finished;
    std::optional<std::string> process;
    std::string id;
    int return_code;
    runtime::Buffer<char> payload;
  };
  std::array<Result, COUNT> saved_results;

  runtime::BufferQueue<char> buffers(10, 1024);

  // FIFO order - we cannot do it in a loop :-(
  EXPECT_CALL(server, invocation_result)
      .WillOnce(testing::DoAll(
          testing::SaveArg<0>(&saved_results[0].process), testing::SaveArg<1>(&saved_results[0].id),
          testing::SaveArg<2>(&saved_results[0].return_code),
          testing::SaveArg<3>(&saved_results[0].payload),
          testing::Invoke([&saved_results]() { saved_results[0].finished.set_value(); })
      ))
      .WillOnce(testing::DoAll(
          testing::SaveArg<0>(&saved_results[1].process), testing::SaveArg<1>(&saved_results[1].id),
          testing::SaveArg<2>(&saved_results[1].return_code),
          testing::SaveArg<3>(&saved_results[1].payload),
          testing::Invoke([&saved_results]() { saved_results[1].finished.set_value(); })
      ))
      .WillOnce(testing::DoAll(
          testing::SaveArg<0>(&saved_results[2].process), testing::SaveArg<1>(&saved_results[2].id),
          testing::SaveArg<2>(&saved_results[2].return_code),
          testing::SaveArg<3>(&saved_results[2].payload),
          testing::Invoke([&saved_results]() { saved_results[2].finished.set_value(); })
      ))
      .WillOnce(testing::DoAll(
          testing::SaveArg<0>(&saved_results[3].process), testing::SaveArg<1>(&saved_results[3].id),
          testing::SaveArg<2>(&saved_results[3].return_code),
          testing::SaveArg<3>(&saved_results[3].payload),
          testing::Invoke([&saved_results]() { saved_results[3].finished.set_value(); })
      ));

  // Submit
  for (int idx = 0; idx < COUNT; ++idx)
  {

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
  for (int idx = 0; idx < COUNT; ++idx) {
    ASSERT_EQ(
        std::future_status::ready,
        saved_results[idx].finished.get_future().wait_for(std::chrono::seconds(3))
    );
  }

  // Results will arrive in a different order because they are executed by different workers.
  std::sort(saved_results.begin(), saved_results.end(),
      [](Result & first, Result & second) -> bool {
        return first.id < second.id;
      }
  );

  // Validate result
  for(int idx = 0; idx < COUNT; ++idx) {
    // Dataplane message
    EXPECT_FALSE(saved_results[idx].process.has_value());
    EXPECT_EQ(saved_results[idx].id, invocation_id[idx]);
    EXPECT_EQ(saved_results[idx].return_code, 0);

    ASSERT_TRUE(saved_results[idx].payload.len > 0);
    int res = get_output(saved_results[idx].payload);
    EXPECT_EQ(res, results[idx]);

  }

}
