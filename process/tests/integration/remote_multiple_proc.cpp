
#include <praas/common/exceptions.hpp>
#include <praas/common/messages.hpp>
#include <praas/process/controller/config.hpp>
#include <praas/process/controller/controller.hpp>
#include <praas/process/controller/remote.hpp>
#include <praas/process/runtime/ipc/messages.hpp>
#include <praas/sdk/process.hpp>

#include "examples/cpp/test.hpp"

#include <exception>
#include <filesystem>
#include <future>
#include <spdlog/spdlog.h>
#include <thread>

#include <boost/iostreams/device/array.hpp>
#include <boost/interprocess/streams/bufferstream.hpp>
#include <boost/iostreams/stream.hpp>
#include <cereal/archives/binary.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <trantor/net/callbacks.h>
#include <trantor/net/TcpClient.h>

using namespace praas::process;

size_t generate_input_binary(std::string key, const runtime::Buffer<char> & buf)
{
  InputMsgKey input{key};
  boost::interprocess::bufferstream stream(buf.data(), buf.size);
  cereal::BinaryOutputArchive archive_out{stream};
  archive_out(input);
  EXPECT_TRUE(stream.good());
  size_t pos = stream.tellp();
  return pos;
}

size_t generate_input_json(std::string key, const runtime::Buffer<char> & buf)
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

class ProcessRemoteServers : public testing::TestWithParam<std::tuple<std::string>> {
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
    cfg.code.language = runtime::functions::string_to_language(std::get<0>(GetParam()));

    cfg.function_workers = workers;
    // process/tests/<exe> -> process
    cfg.deployment_location = std::filesystem::canonical("/proc/self/exe").parent_path().parent_path();

    for(int i = 0; i < PROC_COUNT; ++i) {

      cfg.ipc_name_prefix = std::to_string(i);

      cfg.process_id = "process_" + std::to_string(i);

      controllers[i] = std::make_unique<Controller>(cfg);

      controller_threads[i] = std::thread{&Controller::start, controllers[i].get()};
    }
  }

  void TearDown() override
  {
    for(int i = 0; i < PROC_COUNT; ++i) {
      controllers[i]->shutdown();
      controller_threads[i].join();
    }
  }

  size_t generate_input(std::string key, const runtime::Buffer<char> & buf)
  {
    if(cfg.code.language == runtime::functions::Language::CPP) {
      return generate_input_binary(key, buf);
    } else if(cfg.code.language == runtime::functions::Language::PYTHON) {
      return generate_input_json(key, buf);
    }
    return 0;
  }

  static constexpr int PROC_COUNT = 2;
  config::Controller cfg;
  std::array<std::thread, PROC_COUNT> controller_threads;
  std::array<std::unique_ptr<Controller>, PROC_COUNT> controllers;

  // Results
  static constexpr int INVOC_COUNT = 4;
  std::atomic<int> idx{};

  using timepoint_t = std::chrono::time_point<std::chrono::system_clock>;

  struct Result {
    std::promise<void> finished;
    std::optional<std::string> process;
    std::string id;
    int return_code;
    runtime::Buffer<char> payload;
    timepoint_t timestamp;
  };
  std::array<Result, INVOC_COUNT> saved_results;

  void reset()
  {
    for(int idx = 0; idx < INVOC_COUNT; ++idx) {
      saved_results[idx].process = std::nullopt;
      saved_results[idx].id.clear();
      saved_results[idx].return_code = -1;
      saved_results[idx].payload = runtime::Buffer<char>{};
      saved_results[idx].finished = std::promise<void>{};
    }
  }
};

/**
 * Set up server and connect.
 *
 * Connect data and control plane to each process, distribute updates.
 *
 * Invoke function on each process.
 *
 * Send messages between processes - schedule send_message and get_message.
 *
 * Invoke recursively on the other process.
 */

TEST_P(ProcessRemoteServers, DataPlaneInvocations)
{
  SetUp(1);

  const int BUF_LEN = 1024;
  runtime::BufferQueue<char> buffers(10, 1024);

  std::vector<std::unique_ptr<remote::TCPServer>> servers;
  for(int i = 0; i < PROC_COUNT; ++i) {
    cfg.port = 8080 + i;
    servers.emplace_back(std::make_unique<remote::TCPServer>(*controllers[i].get(), cfg));
    controllers[i]->set_remote(servers.back().get());
    servers.back()->poll();
  }

  std::vector<praas::sdk::Process> processes;
  for(int i = 0; i < PROC_COUNT; ++i) {
    processes.emplace_back(std::string{"localhost"}, 8080 + i);
    ASSERT_TRUE(processes.back().connect());
  }

  const int COUNT = 4;
  std::array<std::string, COUNT> invocation_id = { "first_id", "second_id", "third_id", "another_id" };

  praas::common::message::ApplicationUpdate msg;
  msg.status_change(static_cast<int>(praas::common::Application::Status::ACTIVE));
  msg.process_id(controllers[1]->process_id());
  msg.ip_address("localhost");
  msg.port(8080 + 1);
  processes[0].connection().write_n(msg.bytes(), msg.BUF_SIZE);

  msg.process_id(controllers[0]->process_id());
  msg.ip_address("localhost");
  msg.port(8080);
  processes[1].connection().write_n(msg.bytes(), msg.BUF_SIZE);

  auto buf = buffers.retrieve_buffer(BUF_LEN);
  buf.len = generate_input("msg_key", buf);

  auto result = processes[0].invoke("send_remote_message", invocation_id[idx], buf.data(), buf.len);

  //std::this_thread::sleep_for(std::chrono::milliseconds(500));

  //const int COUNT = 4;
  //int BUF_LEN = 1024;
  //std::string function_name = "add";
  //std::array<std::string, COUNT> invocation_id = { "first_id", "second_id", "third_id", "another_id" };
  //std::array<std::tuple<int, int>, COUNT> args = {
  //  std::make_tuple(42, 4), std::make_tuple(-1, 35),
  //  std::make_tuple(1000, 0), std::make_tuple(-33, 39)
  //};
  //std::array<int, COUNT> results = { 46, 34, 1000, 6 };
  //runtime::BufferQueue<char> buffers(10, BUF_LEN);

  //for(int idx = 0; idx < args.size(); ++idx) {

  //  auto buf = buffers.retrieve_buffer(BUF_LEN);
  //  buf.len = generate_input(std::get<0>(args[idx]), std::get<1>(args[idx]), buf);

  //  auto result = process.invoke(function_name, invocation_id[idx], buf.data(), buf.len);

  //  ASSERT_TRUE(result.payload_len > 0);
  //  int res = get_output(result.payload.get(), result.payload_len);
  //  EXPECT_EQ(res, results[idx]);

  //}

  for(int i = 0; i < PROC_COUNT; ++i) {
    processes[i].disconnect();
  }

  for(int i = 0; i < PROC_COUNT; ++i) {
    servers[i]->shutdown();
  }
}


#if defined(PRAAS_WITH_INVOKER_PYTHON)
  INSTANTIATE_TEST_SUITE_P(ProcessRemoteServers,
                           ProcessRemoteServers,
                           testing::Values("cpp", "python")
                           );
#else
  INSTANTIATE_TEST_SUITE_P(ProcessRemoteServers,
                           ProcessRemoteServers,
                           testing::Values("cpp")
                          );
#endif
