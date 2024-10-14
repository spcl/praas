
#include <praas/common/exceptions.hpp>
#include <praas/common/messages.hpp>
#include <praas/process/controller/config.hpp>
#include <praas/process/controller/controller.hpp>
#include <praas/process/controller/remote.hpp>
#include <praas/process/runtime/internal/ipc/messages.hpp>

#include "examples/cpp/test.hpp"
#include "praas/process/runtime/internal/buffer.hpp"

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

size_t generate_input_binary(int arg1, int arg2, const runtime::internal::Buffer<char>& buf)
{
  Input input{arg1, arg2};
  boost::interprocess::bufferstream stream(buf.data(), buf.size);
  {
    cereal::BinaryOutputArchive archive_out{stream};
    archive_out(cereal::make_nvp("input", input));
    assert(stream.good());
  }
  EXPECT_TRUE(stream.good());
  size_t pos = stream.tellp();
  return pos;
}

size_t generate_input_json(int arg1, int arg2, const runtime::internal::Buffer<char>& buf)
{
  Input input{arg1, arg2};
  boost::interprocess::bufferstream stream(buf.data(), buf.size);
  {
    cereal::JSONOutputArchive archive_out{stream};
    archive_out(cereal::make_nvp("input", input));
    EXPECT_TRUE(stream.good());
  }
  size_t pos = stream.tellp();
  return pos;
}

int get_output_binary(const runtime::internal::Buffer<char>& buf)
{
  Output out;
  boost::iostreams::stream<boost::iostreams::array_source> stream(buf.data(), buf.len);
  cereal::BinaryInputArchive archive_in{stream};
  out.load(archive_in);

  return out.result;
}

int get_output_json(const runtime::internal::Buffer<char>& buf)
{
  Output out;
  boost::iostreams::stream<boost::iostreams::array_source> stream(buf.data(), buf.len);
  cereal::JSONInputArchive archive_in{stream};
  out.load(archive_in);

  return out.result;
}

class ProcessManyWorkersInvocationTest : public testing::TestWithParam<std::string> {
public:
  void SetUp(int workers)
  {
    cfg.set_defaults();
    cfg.verbose = true;

    // Linux specific
    auto path = std::filesystem::canonical("/proc/self/exe").parent_path() / "integration";
    cfg.code.location = path;
    cfg.code.config_location = "configuration.json";
    cfg.code.language = runtime::internal::string_to_language(GetParam());

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

  void reset()
  {
    for (int idx = 0; idx < INVOC_COUNT; ++idx) {
      saved_results[idx].process = std::nullopt;
      saved_results[idx].id.clear();
      saved_results[idx].return_code = -1;
      saved_results[idx].payload = runtime::internal::Buffer<char>{};
      saved_results[idx].finished = std::promise<void>{};
    }
  }

  size_t generate_input(int arg1, int arg2, const runtime::internal::Buffer<char>& buf)
  {
    if (cfg.code.language == runtime::internal::Language::CPP) {
      return generate_input_binary(arg1, arg2, buf);
    } else if (cfg.code.language == runtime::internal::Language::PYTHON) {
      return generate_input_json(arg1, arg2, buf);
    }
    return 0;
  }

  int get_output(const runtime::internal::Buffer<char>& buf)
  {
    if (cfg.code.language == runtime::internal::Language::CPP) {
      return get_output_binary(buf);
    } else if (cfg.code.language == runtime::internal::Language::PYTHON) {
      return get_output_json(buf);
    }
    return -1;
  }

  std::atomic<int> idx{};

  std::thread controller_thread;
  config::Controller cfg;
  std::unique_ptr<Controller> controller;
  MockTCPServer server;

  static constexpr int INVOC_COUNT = 4;

  using timepoint_t = std::chrono::time_point<std::chrono::system_clock>;

  struct Result {
    std::promise<void> finished;
    std::optional<std::string> process;
    std::string id;
    int return_code;
    runtime::internal::Buffer<char> payload;
    timepoint_t timestamp;
  };
  std::array<Result, INVOC_COUNT> saved_results;
};

TEST_P(ProcessManyWorkersInvocationTest, SubsequentInvocations)
{

  SetUp(1);

  const int COUNT = 4;
  const int BUF_LEN = 1024;
  std::string function_name = "add";
  std::string process_id = "remote-process-1";
  std::array<std::string, COUNT> invocation_id = {"first_id", "second_id", "third_id", "fourth_id"};

  std::array<std::tuple<int, int>, COUNT> args = {
      std::make_tuple(42, 4), std::make_tuple(-1, 35), std::make_tuple(1000, 0),
      std::make_tuple(-33, 39)};
  std::array<int, COUNT> results = {46, 34, 1000, 6};

  runtime::internal::BufferQueue<char> buffers(10, 1024);

  // Submit
  for (int idx = 0; idx < COUNT; ++idx) {

    praas::common::message::InvocationRequestData msg;
    msg.function_name(function_name);
    msg.invocation_id(invocation_id[idx]);

    auto buf = buffers.retrieve_buffer(BUF_LEN);
    buf.len = generate_input(std::get<0>(args[idx]), std::get<1>(args[idx]), buf);
    // Send more data than needed - check that it still works
    msg.payload_size(buf.len);

    controller->dataplane_message(std::move(msg.data_buffer()), std::move(buf));
  }

  // wait
  for (int idx = 0; idx < COUNT; ++idx) {
    ASSERT_EQ(
        std::future_status::ready,
        saved_results[idx].finished.get_future().wait_for(std::chrono::seconds(1))
    );
  }

  // Validate result
  for (int idx = 0; idx < COUNT; ++idx) {

    // Dataplane message
    EXPECT_FALSE(saved_results[idx].process.has_value());
    EXPECT_EQ(saved_results[idx].id, invocation_id[idx]);
    EXPECT_EQ(saved_results[idx].return_code, 0);

    ASSERT_TRUE(saved_results[idx].payload.len > 0);
    int res = get_output(saved_results[idx].payload);
    EXPECT_EQ(res, results[idx]);
  }

  // Validate order
  for (int idx = 1; idx < COUNT; ++idx) {
    ASSERT_TRUE(saved_results[idx - 1].timestamp < saved_results[idx].timestamp);
  }
}

TEST_P(ProcessManyWorkersInvocationTest, ConcurrentInvocations)
{
  SetUp(2);

  const int COUNT = 4;
  const int BUF_LEN = 1024;
  std::string function_name = "add";
  std::string process_id = "remote-process-1";
  std::array<std::string, COUNT> invocation_id = {"1_id", "2_id", "3_id", "4_id"};

  std::array<std::tuple<int, int>, COUNT> args = {
      std::make_tuple(42, 4), std::make_tuple(-1, 35), std::make_tuple(1000, 0),
      std::make_tuple(-33, 39)};
  std::array<int, COUNT> results = {46, 34, 1000, 6};

  runtime::internal::BufferQueue<char> buffers(10, 1024);

  // Submit
  for (int idx = 0; idx < COUNT; ++idx) {

    praas::common::message::InvocationRequestData msg;
    msg.function_name(function_name);
    msg.invocation_id(invocation_id[idx]);

    auto buf = buffers.retrieve_buffer(BUF_LEN);
    buf.len = generate_input(std::get<0>(args[idx]), std::get<1>(args[idx]), buf);
    // Send more data than needed - check that it still works
    msg.payload_size(buf.len + 64);

    controller->dataplane_message(std::move(msg.data_buffer()), std::move(buf));
  }

  // wait
  for (int idx = 0; idx < COUNT; ++idx) {
    ASSERT_EQ(
        std::future_status::ready,
        saved_results[idx].finished.get_future().wait_for(std::chrono::seconds(1))
    );
  }

  // Results will arrive in a different order because they are executed by different workers.
  std::sort(saved_results.begin(), saved_results.end(), [](Result& first, Result& second) -> bool {
    return first.id < second.id;
  });

  // Validate result
  for (int idx = 0; idx < COUNT; ++idx) {
    // Dataplane message
    EXPECT_FALSE(saved_results[idx].process.has_value());
    EXPECT_EQ(saved_results[idx].id, invocation_id[idx]);
    EXPECT_EQ(saved_results[idx].return_code, 0);

    ASSERT_TRUE(saved_results[idx].payload.len > 0);
    int res = get_output(saved_results[idx].payload);
    EXPECT_EQ(res, results[idx]);
  }
}

#if defined(PRAAS_WITH_INVOKER_PYTHON)
INSTANTIATE_TEST_SUITE_P(
    ProcessManyWorkersInvocationTest, ProcessManyWorkersInvocationTest,
    testing::Values("cpp", "python")
);
#else
INSTANTIATE_TEST_SUITE_P(
    ProcessManyWorkersInvocationTest, ProcessManyWorkersInvocationTest, testing::Values("cpp")
);
#endif
