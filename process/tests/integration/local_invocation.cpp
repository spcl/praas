
#include <praas/common/exceptions.hpp>
#include <praas/common/messages.hpp>
#include <praas/process/controller/config.hpp>
#include <praas/process/controller/controller.hpp>
#include <praas/process/controller/remote.hpp>
#include <praas/process/runtime/internal/functions.hpp>
#include <praas/process/runtime/internal/ipc/messages.hpp>

#include "examples/cpp/test.hpp"

#include <boost/iostreams/device/array.hpp>
#include <filesystem>
#include <future>
#include <thread>

#include <boost/interprocess/streams/bufferstream.hpp>
#include <boost/iostreams/stream.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace praas::process;

class MockTCPServer : public remote::Server {
public:
  MockTCPServer() = default;

  MOCK_METHOD(void, poll, (std::optional<std::string> control_plane_address), (override));
  MOCK_METHOD(
      void, put_message, (std::string_view, std::string_view, runtime::internal::Buffer<char>&&),
      (override)
  );
  MOCK_METHOD(
      void, invocation_result,
      (remote::RemoteType, std::optional<std::string_view>, std::string_view, int,
       runtime::internal::BufferAccessor<char>),
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
  cereal::BinaryOutputArchive archive_out{stream};
  archive_out(cereal::make_nvp("input", input));
  assert(stream.good());
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
    assert(stream.good());
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

class ProcessLocalInvocationTest : public testing::TestWithParam<std::string> {
public:
  void SetUp() override
  {
    cfg.set_defaults();
    cfg.verbose = true;

    // Linux specific
    auto path = std::filesystem::canonical("/proc/self/exe").parent_path() / "integration";
    cfg.code.location = path;
    cfg.code.config_location = "configuration.json";
    cfg.code.language = runtime::internal::string_to_language(GetParam());

    // process/tests/<exe> -> process
    cfg.deployment_location =
        std::filesystem::canonical("/proc/self/exe").parent_path().parent_path();

    cfg.function_workers = 4;

    controller = std::make_unique<Controller>(cfg);
    controller->set_remote(&server);

    controller_thread = std::thread{&Controller::start, controller.get()};

    EXPECT_CALL(server, invocation_result)
        .WillRepeatedly([&](remote::RemoteType, auto _process, auto _id, int _return_code,
                            auto&& _payload) {
          process = _process;
          id = _id;
          return_code = _return_code;
          payload = _payload.copy();
          finished.set_value();
        });
  }

  void TearDown() override
  {
    controller->shutdown();
    controller_thread.join();
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

  std::thread controller_thread;
  config::Controller cfg;
  std::unique_ptr<Controller> controller;
  MockTCPServer server;

  // Results
  std::promise<void> finished;
  std::optional<std::string> process;
  std::string id;
  int return_code;
  runtime::internal::Buffer<char> payload;

  void reset()
  {
    process = std::nullopt;
    id.clear();
    return_code = -1;
    payload = runtime::internal::Buffer<char>{};
    finished = std::promise<void>{};
  }
};

/**
 * Four workers.
 *
 * Invoke one, two, and three recursive functions.
 */

TEST_P(ProcessLocalInvocationTest, SimpleInvocation)
{
  const int BUF_LEN = 1024;
  std::string function_name = "power";
  std::string process_id = "remote-process-1";
  std::array<std::string, 3> invocation_id = {"first_id", "second_id", "third_id"};

  std::array<std::tuple<int, int>, 3> args = {
      std::make_tuple(2, 3), std::make_tuple(2, 4), std::make_tuple(3, 5)};
  std::array<int, 3> results = {8, 16, 243};

  runtime::internal::BufferQueue<char> buffers(10, 1024);

  for (int i = 0; i < invocation_id.size(); ++i) {

    reset();

    praas::common::message::InvocationRequest msg;
    msg.function_name(function_name);
    msg.invocation_id(invocation_id[i]);

    auto buf = buffers.retrieve_buffer(BUF_LEN);
    buf.len = generate_input(std::get<0>(args[i]), std::get<1>(args[i]), buf);
    // Send more data than needed - check that it still works
    msg.payload_size(buf.len + 64);

    controller->dataplane_message(std::move(msg), std::move(buf));

    // Wait for the invocation to finish
    ASSERT_EQ(std::future_status::ready, finished.get_future().wait_for(std::chrono::seconds(1)));

    // Dataplane message
    EXPECT_FALSE(process.has_value());
    EXPECT_EQ(id, invocation_id[i]);
    EXPECT_EQ(return_code, 0);

    ASSERT_TRUE(payload.len > 0);
    int res = get_output(payload);
    EXPECT_EQ(res, results[i]);
  }
}

#if defined(PRAAS_WITH_INVOKER_PYTHON)
INSTANTIATE_TEST_SUITE_P(
    ProcessLocalInvocationTest, ProcessLocalInvocationTest, testing::Values("cpp", "python")
);
#else
INSTANTIATE_TEST_SUITE_P(
    ProcessLocalInvocationTest, ProcessLocalInvocationTest, testing::Values("cpp")
);
#endif
