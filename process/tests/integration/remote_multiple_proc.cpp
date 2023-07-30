
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

size_t generate_input_key_binary(std::string key, const runtime::internal::Buffer<char>& buf)
{
  InputMsgKey input{key};
  boost::interprocess::bufferstream stream(buf.data(), buf.size);
  cereal::BinaryOutputArchive archive_out{stream};
  archive_out(input);
  EXPECT_TRUE(stream.good());
  size_t pos = stream.tellp();
  return pos;
}

size_t generate_input_key_json(std::string key, const runtime::internal::Buffer<char>& buf)
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

size_t generate_input_add_binary(int arg1, int arg2, const runtime::internal::Buffer<char>& buf)
{
  Input input{arg1, arg2};
  boost::interprocess::bufferstream stream(buf.data(), buf.size);
  cereal::BinaryOutputArchive archive_out{stream};
  archive_out(cereal::make_nvp("input", input));
  assert(stream.good());
  size_t pos = stream.tellp();
  return pos;
}

size_t generate_input_add_json(int arg1, int arg2, const runtime::internal::Buffer<char>& buf)
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

int get_output_add_binary(char* ptr, size_t len)
{
  Output out;
  boost::iostreams::stream<boost::iostreams::array_source> stream(ptr, len);
  cereal::BinaryInputArchive archive_in{stream};
  out.load(archive_in);

  return out.result;
}

int get_output_add_json(char* ptr, size_t len)
{
  Output out;
  boost::iostreams::stream<boost::iostreams::array_source> stream(ptr, len);
  cereal::JSONInputArchive archive_in{stream};
  out.load(archive_in);

  return out.result;
}

class ProcessRemoteServers : public testing::TestWithParam<std::tuple<std::string>> {
public:
  void SetUp(int workers)
  {
    cfg.set_defaults();
    cfg.verbose = true;

    spdlog::set_pattern("[%H:%M:%S:%f] [%n] [P %P] [T %t] [%l] %v ");
    spdlog::set_level(spdlog::level::debug);

    // Linux specific
    auto path = std::filesystem::canonical("/proc/self/exe").parent_path() / "integration";
    cfg.code.location = path;
    cfg.code.config_location = "configuration.json";
    cfg.code.language = runtime::internal::string_to_language(std::get<0>(GetParam()));

    cfg.function_workers = workers;
    // process/tests/<exe> -> process
    cfg.deployment_location =
        std::filesystem::canonical("/proc/self/exe").parent_path().parent_path();

    for (int i = 0; i < PROC_COUNT; ++i) {

      cfg.ipc_name_prefix = std::to_string(i);

      cfg.process_id = "process_" + std::to_string(i);

      controllers[i] = std::make_unique<Controller>(cfg);

      controller_threads[i] = std::thread{&Controller::start, controllers[i].get()};
    }
  }

  void TearDown() override
  {
    for (int i = 0; i < PROC_COUNT; ++i) {
      controllers[i]->shutdown();
      controller_threads[i].join();
    }
  }

  size_t generate_input_key(std::string key, const runtime::internal::Buffer<char>& buf)
  {
    if (cfg.code.language == runtime::internal::Language::CPP) {
      return generate_input_key_binary(key, buf);
    } else if (cfg.code.language == runtime::internal::Language::PYTHON) {
      return generate_input_key_json(key, buf);
    }
    return 0;
  }

  size_t generate_input_add(int arg1, int arg2, const runtime::internal::Buffer<char>& buf)
  {
    if (cfg.code.language == runtime::internal::Language::CPP) {
      return generate_input_add_binary(arg1, arg2, buf);
    } else if (cfg.code.language == runtime::internal::Language::PYTHON) {
      return generate_input_add_json(arg1, arg2, buf);
    }
    return 0;
  }

  int get_output_add(char* ptr, size_t len)
  {
    if (cfg.code.language == runtime::internal::Language::CPP) {
      return get_output_add_binary(ptr, len);
    } else if (cfg.code.language == runtime::internal::Language::PYTHON) {
      return get_output_add_json(ptr, len);
    }
    return -1;
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

TEST_P(ProcessRemoteServers, GetPutCommunication)
{
  SetUp(1);

  const int BUF_LEN = 1024;
  runtime::internal::BufferQueue<char> buffers(10, 1024);

  std::vector<std::unique_ptr<remote::TCPServer>> servers;
  for (int i = 0; i < PROC_COUNT; ++i) {
    cfg.port = 8080 + i;
    servers.emplace_back(std::make_unique<remote::TCPServer>(*controllers[i].get(), cfg));
    controllers[i]->set_remote(servers.back().get());
    servers.back()->poll();
  }

  std::vector<praas::sdk::Process> processes;
  for (int i = 0; i < PROC_COUNT; ++i) {
    processes.emplace_back(std::string{"localhost"}, 8080 + i);
    ASSERT_TRUE(processes.back().connect());
  }

  const int COUNT = 4;
  std::array<std::string, COUNT> invocation_id = {
      "first_id", "second_id", "third_id", "another_id"};

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
  buf.len = generate_input_key("msg_key", buf);

  auto result = processes[0].invoke("send_remote_message", invocation_id[idx], buf.data(), buf.len);
  // Wait to ensure that message is propagated.
  std::this_thread::sleep_for(std::chrono::milliseconds(250));
  auto result_get =
      processes[1].invoke("get_remote_message", invocation_id[idx], buf.data(), buf.len);

  ASSERT_EQ(result.return_code, 0);
  ASSERT_EQ(result_get.return_code, 0);

  spdlog::info("");
  for (int i = 0; i < 4; ++i) {
    spdlog::info("-----------------------------------------------");
  }
  spdlog::info("");

  // Now reverse - we first put pending messages everywhere, then ensure that data is delivered
  // later. Furthermore, we reverse the order of communication: from process 1 -> to 0.
  std::thread nonblocking{[&]() mutable {
    result_get = processes[0].invoke("get_remote_message", invocation_id[idx], buf.data(), buf.len);
  }};
  // Wait to ensure that message is propagated.
  std::this_thread::sleep_for(std::chrono::milliseconds(250));
  result = processes[1].invoke("send_remote_message", invocation_id[idx], buf.data(), buf.len);
  nonblocking.join();

  ASSERT_EQ(result.return_code, 0);
  ASSERT_EQ(result_get.return_code, 0);

  for (int i = 0; i < PROC_COUNT; ++i) {
    processes[i].disconnect();
  }

  for (int i = 0; i < PROC_COUNT; ++i) {
    servers[i]->shutdown();
  }
}

TEST_P(ProcessRemoteServers, SimultaenousMessaging)
{
  SetUp(2);

  const int BUF_LEN = 1024;
  runtime::internal::BufferQueue<char> buffers(10, 1024);

  std::vector<std::unique_ptr<remote::TCPServer>> servers;
  for (int i = 0; i < PROC_COUNT; ++i) {
    cfg.port = 8080 + i;
    servers.emplace_back(std::make_unique<remote::TCPServer>(*controllers[i].get(), cfg));
    controllers[i]->set_remote(servers.back().get());
    servers.back()->poll();
  }

  std::vector<praas::sdk::Process> processes;
  for (int i = 0; i < PROC_COUNT; ++i) {
    processes.emplace_back(std::string{"localhost"}, 8080 + i);
    ASSERT_TRUE(processes.back().connect());
  }

  const int COUNT = 4;
  std::array<std::string, COUNT> invocation_id = {
      "first_id", "second_id", "third_id", "another_id"};

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
  buf.len = generate_input_key("msg_key", buf);

  praas::sdk::InvocationResult first, second;

  std::thread first_thread{[&]() mutable {
    first = processes[0].invoke("put_get_remote_message", invocation_id[idx], buf.data(), buf.len);
  }};
  std::thread second_thread{[&]() mutable {
    second = processes[1].invoke("put_get_remote_message", invocation_id[idx], buf.data(), buf.len);
  }};

  first_thread.join(), second_thread.join();

  ASSERT_EQ(first.return_code, 0);
  ASSERT_EQ(second.return_code, 0);

  spdlog::info("");
  for (int i = 0; i < 4; ++i) {
    spdlog::info("-----------------------------------------------");
  }
  spdlog::info("");

  for (int i = 0; i < PROC_COUNT; ++i) {
    processes[i].disconnect();
  }

  for (int i = 0; i < PROC_COUNT; ++i) {
    servers[i]->shutdown();
  }
}

TEST_P(ProcessRemoteServers, RemoteInvocations)
{
  SetUp(2);

  const int BUF_LEN = 1024;
  runtime::internal::BufferQueue<char> buffers(10, 1024);

  std::vector<std::unique_ptr<remote::TCPServer>> servers;
  for (int i = 0; i < PROC_COUNT; ++i) {
    cfg.port = 8080 + i;
    servers.emplace_back(std::make_unique<remote::TCPServer>(*controllers[i].get(), cfg));
    controllers[i]->set_remote(servers.back().get());
    servers.back()->poll();
  }

  std::vector<praas::sdk::Process> processes;
  for (int i = 0; i < PROC_COUNT; ++i) {
    processes.emplace_back(std::string{"localhost"}, 8080 + i);
    ASSERT_TRUE(processes.back().connect());
  }

  const int COUNT = 2;
  std::array<std::tuple<int, int>, COUNT> args = {std::make_tuple(42, 4), std::make_tuple(-1, 35)};
  std::array<int, COUNT> results = {46 * 2, 34 * 2};
  std::array<praas::sdk::InvocationResult, COUNT> invoc_results;
  std::array<std::string, COUNT> invocation_id = {"first_id", "second_id"};

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

  std::vector<std::thread> invoc_threads;
  for (int idx = 0; idx < COUNT; ++idx) {
    auto buf = buffers.retrieve_buffer(BUF_LEN);
    buf.len = generate_input_add(std::get<0>(args[idx]), std::get<1>(args[idx]), buf);

    invoc_threads.emplace_back([&, idx, buf = std::move(buf)]() mutable {
      invoc_results[idx] =
          processes[idx].invoke("remote_invocation", invocation_id[idx], buf.data(), buf.len);
    });
  }

  for (int idx = 0; idx < COUNT; ++idx) {
    invoc_threads[idx].join();
  }

  for (int idx = 0; idx < COUNT; ++idx) {

    ASSERT_EQ(invoc_results[idx].return_code, 0);
    ASSERT_TRUE(invoc_results[idx].payload_len > 0);
    int res = get_output_add(invoc_results[idx].payload.get(), invoc_results[idx].payload_len);
    EXPECT_EQ(res, results[idx]);
  }

  for (int i = 0; i < PROC_COUNT; ++i) {
    processes[i].disconnect();
  }

  for (int i = 0; i < PROC_COUNT; ++i) {
    servers[i]->shutdown();
  }
}

TEST_P(ProcessRemoteServers, RemoteInvocationsUnknown)
{
  SetUp(2);

  const int BUF_LEN = 1024;
  runtime::internal::BufferQueue<char> buffers(10, 1024);

  std::vector<std::unique_ptr<remote::TCPServer>> servers;
  for (int i = 0; i < PROC_COUNT; ++i) {
    cfg.port = 8080 + i;
    servers.emplace_back(std::make_unique<remote::TCPServer>(*controllers[i].get(), cfg));
    controllers[i]->set_remote(servers.back().get());
    servers.back()->poll();
  }

  std::vector<praas::sdk::Process> processes;
  for (int i = 0; i < PROC_COUNT; ++i) {
    processes.emplace_back(std::string{"localhost"}, 8080 + i);
    ASSERT_TRUE(processes.back().connect());
  }

  const int COUNT = 2;
  std::array<std::tuple<int, int>, COUNT> args = {std::make_tuple(42, 4), std::make_tuple(-1, 35)};
  std::array<int, COUNT> results = {46 * 2, 34 * 2};
  std::array<praas::sdk::InvocationResult, COUNT> invoc_results;
  std::array<std::string, COUNT> invocation_id = {"first_id", "second_id"};

  // FIXME: add a way to automatically handle that
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

  std::vector<std::thread> invoc_threads;
  for (int idx = 0; idx < COUNT; ++idx) {
    auto buf = buffers.retrieve_buffer(BUF_LEN);
    buf.len = generate_input_add(std::get<0>(args[idx]), std::get<1>(args[idx]), buf);

    invoc_threads.emplace_back([&, idx, buf = std::move(buf)]() mutable {
      invoc_results[idx] = processes[idx].invoke(
          "remote_invocation_unknown", invocation_id[idx], buf.data(), buf.len
      );
    });
  }

  for (int idx = 0; idx < COUNT; ++idx) {
    invoc_threads[idx].join();
  }

  for (int idx = 0; idx < COUNT; ++idx) {
    ASSERT_EQ(invoc_results[idx].return_code, 0);
    ASSERT_EQ(invoc_results[idx].payload_len, 0);
    EXPECT_TRUE(invoc_results[idx].error_message.empty());
  }

  for (int i = 0; i < PROC_COUNT; ++i) {
    processes[i].disconnect();
  }

  for (int i = 0; i < PROC_COUNT; ++i) {
    servers[i]->shutdown();
  }
}

#if defined(PRAAS_WITH_INVOKER_PYTHON)
INSTANTIATE_TEST_SUITE_P(
    ProcessRemoteServers, ProcessRemoteServers, testing::Values("cpp", "python")
);
#else
INSTANTIATE_TEST_SUITE_P(ProcessRemoteServers, ProcessRemoteServers, testing::Values("cpp"));
#endif
