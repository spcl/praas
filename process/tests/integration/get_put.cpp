
#include <praas/common/exceptions.hpp>
#include <praas/common/messages.hpp>
#include <praas/process/controller/config.hpp>
#include <praas/process/controller/controller.hpp>
#include <praas/process/controller/remote.hpp>
#include <praas/process/runtime/internal/ipc/messages.hpp>

#include "examples/cpp/test.hpp"

#include <filesystem>
#include <future>
#include <thread>

#include <boost/interprocess/streams/bufferstream.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>
#include <cereal/archives/binary.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace praas::process;

class MockTCPServer : public remote::Server {
public:
  MockTCPServer() = default;

  MOCK_METHOD(void, poll, (std::optional<std::string>), (override));
  MOCK_METHOD(
      void, put_message, (std::string_view, std::string_view, runtime::internal::Buffer<char>&&),
      (override)
  );
  MOCK_METHOD(
      void, swap_confirmation, (size_t, double),
      (override)
  );
  MOCK_METHOD(
      void, invocation_result,
      (remote::RemoteType, std::optional<std::string_view>, std::string_view, int,
       runtime::internal::BufferAccessor<const char>),
      (override)
  );
  MOCK_METHOD(
      void, invocation_request,
      (std::string_view, std::string_view, std::string_view, runtime::internal::Buffer<char>&&),
      (override)
  );
};

size_t generate_input_binary(std::string key, const runtime::internal::Buffer<char>& buf)
{
  InputMsgKey input{key};
  boost::interprocess::bufferstream stream(buf.data(), buf.size);
  cereal::BinaryOutputArchive archive_out{stream};
  archive_out(input);
  EXPECT_TRUE(stream.good());
  size_t pos = stream.tellp();
  return pos;
}

size_t generate_input_json(std::string key, const runtime::internal::Buffer<char>& buf)
{
  InputMsgKey input{key};
  boost::interprocess::bufferstream stream(buf.data(), buf.size);
  {
    cereal::JSONOutputArchive archive_out{stream};
    archive_out(cereal::make_nvp("input", input));
    EXPECT_TRUE(stream.good());
  }
  size_t pos = stream.tellp();
  return pos;
}

class ProcessMessagingTest : public testing::TestWithParam<std::tuple<std::string, std::string>> {
public:
  void SetUp(int workers)
  {
    cfg.set_defaults();
    cfg.verbose = true;

    // Linux specific
    auto path = std::filesystem::canonical("/proc/self/exe").parent_path() / "integration";
    cfg.code.location = path;
    cfg.code.config_location = "configuration.json";
    cfg.code.language = runtime::internal::string_to_language(std::get<1>(GetParam()));

    cfg.function_workers = workers;
    // process/tests/<exe> -> process
    cfg.deployment_location =
        std::filesystem::canonical("/proc/self/exe").parent_path().parent_path();

    controller = std::make_unique<Controller>(cfg);
    controller->set_remote(&server);

    controller_thread = std::thread{&Controller::start, controller.get()};

    EXPECT_CALL(server, invocation_result)
        .WillRepeatedly([&](auto, auto _process, auto _id, int _return_code,
                            auto&& _payload) mutable {
          saved_results[idx].process = _process;
          saved_results[idx].id = _id;
          saved_results[idx].return_code = _return_code;
          saved_results[idx].payload = _payload.copy();
          saved_results[idx].finished.set_value();

          saved_results[idx].timestamp = std::chrono::system_clock::now();

          idx++;
        });
  }

  void TearDown() override
  {
    controller->shutdown();
    controller_thread.join();
  }

  size_t generate_input(std::string key, const runtime::internal::Buffer<char>& buf)
  {
    if (cfg.code.language == runtime::internal::Language::CPP) {
      return generate_input_binary(key, buf);
    } else if (cfg.code.language == runtime::internal::Language::PYTHON) {
      return generate_input_json(key, buf);
    }
    return 0;
  }

  std::thread controller_thread;
  config::Controller cfg;
  std::unique_ptr<Controller> controller;
  MockTCPServer server;

  // Results
  static constexpr int INVOC_COUNT = 4;
  std::atomic<int> idx{};

  using timepoint_t = std::chrono::time_point<std::chrono::system_clock>;

  struct Result {
    std::promise<void> finished;
    std::optional<std::string> process;
    std::string id;
    int return_code;
    runtime::internal::BufferAccessor<char> payload;
    timepoint_t timestamp;
  };
  std::array<Result, INVOC_COUNT> saved_results;

  void reset()
  {
    for (int idx = 0; idx < INVOC_COUNT; ++idx) {
      saved_results[idx].process = std::nullopt;
      saved_results[idx].id.clear();
      saved_results[idx].return_code = -1;
      saved_results[idx].payload = runtime::internal::BufferAccessor<char>{};
      saved_results[idx].finished = std::promise<void>{};
    }
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

  SetUp(1);

  const int BUF_LEN = 1024;
  std::string put_function_name = "send_message";
  std::string get_function_name = std::get<0>(GetParam());
  std::string process_id = "remote-process-1";
  std::array<std::string, 2> invocation_id = {"first_id", "second_id"};

  runtime::internal::BufferQueue<char> buffers(10, 1024);

  reset();

  // First invocation
  {
    int idx = 0;
    praas::common::message::InvocationRequestData msg;
    msg.function_name(put_function_name);
    msg.invocation_id(invocation_id[idx]);

    auto buf = buffers.retrieve_buffer(BUF_LEN);
    buf.len = generate_input("msg_key", buf);

    controller->dataplane_message(std::move(msg.data_buffer()), std::move(buf));

    // Wait for the invocation to finish
    ASSERT_EQ(
        std::future_status::ready,
        saved_results[0].finished.get_future().wait_for(std::chrono::seconds(1))
    );

    // Dataplane message
    EXPECT_FALSE(saved_results[0].process.has_value());
    EXPECT_EQ(saved_results[0].id, invocation_id[idx]);
    EXPECT_EQ(saved_results[0].return_code, 0);
  }

  reset();

  // Second invocation
  {
    int idx = 1;
    praas::common::message::InvocationRequestData msg;
    msg.function_name(get_function_name);
    msg.invocation_id(invocation_id[idx]);

    auto buf = buffers.retrieve_buffer(BUF_LEN);
    buf.len = generate_input("msg_key", buf);

    controller->remote_message(std::move(msg.data_buffer()), std::move(buf), process_id);

    // Wait for the invocation to finish
    ASSERT_EQ(
        std::future_status::ready,
        saved_results[1].finished.get_future().wait_for(std::chrono::seconds(1))
    );

    // Remote message
    EXPECT_TRUE(saved_results[1].process.has_value());
    EXPECT_EQ(saved_results[1].process.value(), process_id);
    EXPECT_EQ(saved_results[1].id, invocation_id[idx]);
    EXPECT_EQ(saved_results[1].return_code, 0);
  }
}

#if defined(PRAAS_WITH_INVOKER_PYTHON)
INSTANTIATE_TEST_SUITE_P(
    ProcessGetPutTestSelf, ProcessMessagingTest,
    testing::Combine(
        testing::Values("get_message_self", "get_message_any", "get_message_explicit"),
        testing::Values("cpp", "python")
    )
);
#else
INSTANTIATE_TEST_SUITE_P(
    ProcessGetPutTestSelf, ProcessMessagingTest,
    testing::Combine(
        testing::Values("get_message_self", "get_message_any", "get_message_explicit"),
        testing::Values("cpp")
    )
);
#endif

/**
 * (1) Get message blocking, (2) function puts a message that activates it.
 *
 * (1) Put message, (2) receive from self.
 *
 * (1) Put message, (2) receive from any.
 */

TEST_P(ProcessMessagingTest, GetPutTwoWorkers)
{
  SetUp(2);

  const int BUF_LEN = 1024;
  std::string put_function_name = "send_message";
  std::string get_function_name = "get_message_self";
  std::string process_id = "remote-process-1";
  std::array<std::string, 2> invocation_id = {"first_id", "second_id"};

  runtime::internal::BufferQueue<char> buffers(10, 1024);

  reset();

  // First invocation
  praas::common::message::InvocationRequestData msg;
  msg.function_name(get_function_name);
  msg.invocation_id(invocation_id[0]);

  auto buf = buffers.retrieve_buffer(BUF_LEN);
  buf.len = generate_input("msg_key", buf);

  controller->dataplane_message(std::move(msg.data_buffer()), std::move(buf));

  // Ensure that `get` function is running.
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  praas::common::message::InvocationRequestData put_msg;
  put_msg.function_name(put_function_name);
  put_msg.invocation_id(invocation_id[1]);

  buf = buffers.retrieve_buffer(BUF_LEN);
  buf.len = generate_input("msg_key", buf);

  controller->dataplane_message(std::move(put_msg.data_buffer()), std::move(buf));

  // Wait for both invocations to finish
  for (int i = 0; i < 2; ++i) {
    ASSERT_EQ(
        std::future_status::ready,
        saved_results[i].finished.get_future().wait_for(std::chrono::seconds(1))
    );
  }

  // Results will arrive in a different order because they are executed by different workers.
  std::sort(
      saved_results.begin(), saved_results.begin() + 2,
      [](Result& first, Result& second) -> bool { return first.id < second.id; }
  );

  for (int i = 0; i < 2; ++i) {
    // Dataplane message
    EXPECT_FALSE(saved_results[i].process.has_value());
    EXPECT_EQ(saved_results[i].id, invocation_id[i]);
    EXPECT_EQ(saved_results[i].return_code, 0);
    EXPECT_EQ(saved_results[i].return_code, 0);
  }
}
