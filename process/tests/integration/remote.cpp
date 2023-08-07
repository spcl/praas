
#include <praas/common/exceptions.hpp>
#include <praas/common/messages.hpp>
#include <praas/process/controller/config.hpp>
#include <praas/process/controller/controller.hpp>
#include <praas/process/controller/remote.hpp>
#include <praas/process/runtime/internal/ipc/messages.hpp>
#include <praas/sdk/process.hpp>

#include "examples/cpp/test.hpp"

#include <exception>
#include <filesystem>
#include <future>
#include <spdlog/spdlog.h>
#include <thread>

#include <boost/interprocess/streams/bufferstream.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>
#include <cereal/archives/binary.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <trantor/net/TcpClient.h>
#include <trantor/net/callbacks.h>

using namespace praas::process;

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

int get_output_binary(char* buf, size_t len)
{
  Output out;
  boost::iostreams::stream<boost::iostreams::array_source> stream(buf, len);
  cereal::BinaryInputArchive archive_in{stream};
  out.load(archive_in);

  return out.result;
}

int get_output_json(char* buf, size_t len)
{
  Output out;
  boost::iostreams::stream<boost::iostreams::array_source> stream(buf, len);
  cereal::JSONInputArchive archive_in{stream};
  out.load(archive_in);

  return out.result;
}

class ProcessRemoteServer : public testing::TestWithParam<std::tuple<std::string>> {
public:
  void SetUp(int workers)
  {
    cfg.set_defaults();
    cfg.verbose = true;

    spdlog::set_pattern("[%H:%M:%S:%f] [P %P] [T %t] [%l] %v ");

    // Linux specific
    auto path = std::filesystem::canonical("/proc/self/exe").parent_path() / "integration";
    cfg.code.location = path;
    cfg.code.config_location = "configuration.json";
    cfg.code.language = runtime::internal::string_to_language(std::get<0>(GetParam()));

    cfg.function_workers = workers;
    // process/tests/<exe> -> process
    cfg.deployment_location =
        std::filesystem::canonical("/proc/self/exe").parent_path().parent_path();

    cfg.port = DEFAULT_CONTROLLER_PORT;
    controller = std::make_unique<Controller>(cfg);

    controller_thread = std::thread{&Controller::start, controller.get()};
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

  int get_output(char* buf, size_t len)
  {
    if (cfg.code.language == runtime::internal::Language::CPP) {
      return get_output_binary(buf, len);
    } else if (cfg.code.language == runtime::internal::Language::PYTHON) {
      return get_output_json(buf, len);
    }
    return -1;
  }

  std::thread controller_thread;
  config::Controller cfg;
  std::unique_ptr<Controller> controller;

  static constexpr int DEFAULT_CONTROLLER_PORT = 8200;

  // Results
  static constexpr int INVOC_COUNT = 4;
  std::atomic<int> idx{};

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
};

struct ProcessConnection {

private:
  trantor::EventLoopThread loop;
};

/**
 * Set up server and connect.
 *
 * Connect data plane, invoke function.
 *
 * Connect second process, establish connections.
 *
 * Invoke functions between processes.
 */

TEST_P(ProcessRemoteServer, DataPlaneInvocations)
{
  SetUp(1);

  remote::TCPServer server{*controller.get(), cfg};
  controller->set_remote(&server);
  server.poll();

  praas::sdk::Process process{"localhost", DEFAULT_CONTROLLER_PORT};

  ASSERT_TRUE(process.connect());

  const int COUNT = 4;
  int BUF_LEN = 1024;
  std::string function_name = "add";
  std::array<std::string, COUNT> invocation_id = {
      "first_id", "second_id", "third_id", "another_id"};
  std::array<std::tuple<int, int>, COUNT> args = {
      std::make_tuple(42, 4), std::make_tuple(-1, 35), std::make_tuple(1000, 0),
      std::make_tuple(-33, 39)};
  std::array<int, COUNT> results = {46, 34, 1000, 6};
  runtime::internal::BufferQueue<char> buffers(10, BUF_LEN);

  for (int idx = 0; idx < args.size(); ++idx) {

    auto buf = buffers.retrieve_buffer(BUF_LEN);
    buf.len = generate_input(std::get<0>(args[idx]), std::get<1>(args[idx]), buf);

    auto result = process.invoke(function_name, invocation_id[idx], buf.data(), buf.len);

    ASSERT_TRUE(result.payload_len > 0);
    int res = get_output(result.payload.get(), result.payload_len);
    EXPECT_EQ(res, results[idx]);
  }

  process.disconnect();

  server.shutdown();
}

#if defined(PRAAS_WITH_INVOKER_PYTHON)
INSTANTIATE_TEST_SUITE_P(
    ProcessRemoteServer, ProcessRemoteServer, testing::Values("cpp", "python")
);
#else
INSTANTIATE_TEST_SUITE_P(ProcessRemoteServer, ProcessRemoteServer, testing::Values("cpp"));
#endif
