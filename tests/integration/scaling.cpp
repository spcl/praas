
#include <praas/common/http.hpp>
#include <praas/control-plane/config.hpp>
#include <praas/control-plane/server.hpp>
#include <praas/sdk/praas.hpp>

#include <boost/interprocess/streams/bufferstream.hpp>
#include <chrono>
#include <thread>

#include <boost/iostreams/stream.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

struct Result {
  std::vector<std::string> active;
  std::vector<std::string> swapped;

  template <typename Ar>
  void save(Ar& archive) const
  {
    archive(CEREAL_NVP(active));
    archive(CEREAL_NVP(swapped));
  }

  template <typename Ar>
  void load(Ar& archive)
  {
    archive(CEREAL_NVP(active));
    archive(CEREAL_NVP(swapped));
  }
};

class ScalingApplication : public ::testing::Test {};

std::string get_input_binary(Result& msg)
{
  std::stringstream str;

  //boost::interprocess::bufferstream out_stream(reinterpret_cast<char*>(result.data()), 1024);
  cereal::BinaryOutputArchive archive_out{str};
  archive_out(msg);

  return str.str();
}

TEST_F(ScalingApplication, ExistingProcessUpdate)
{
  praas::common::http::HTTPClientFactory::initialize(1);
  {
    praas::sdk::PraaS praas{fmt::format("http://127.0.0.1:{}", 9000)};

    std::string app_name("test_scaling");
    ASSERT_TRUE(
      praas.create_application(app_name, "spcleth/praas:proc-local")
    );

    int procs = 2;
    std::vector<praas::sdk::Process> processes;
    std::vector<std::future<std::optional<praas::sdk::Process>>> futures;

    auto res = praas.create_process(app_name, std::to_string(0), "1", "1024");
    ASSERT_TRUE(res.has_value());
    processes.push_back(std::move(res.value()));

    //std::this_thread::sleep(std::chrono::milliseconds(250));

    auto res2 = praas.create_process(app_name, std::to_string(1), "1", "1024");
    ASSERT_TRUE(res2.has_value());
    processes.push_back(std::move(res2.value()));

    for(int i = 0; i < procs; ++i) {
      ASSERT_TRUE(processes[i].connect());
    }

    praas::sdk::Process::pool.configure(8);

    Result msg;
    msg.active.emplace_back(std::to_string(0));
    msg.active.emplace_back(std::to_string(1));
    auto input_data = get_input_binary(msg);

    // Notification that the second process was created.

    auto fut = processes[0].invoke_async("world-check", "invocation-id", input_data.data(), input_data.size());
    auto ret = fut.get();
    ASSERT_EQ(ret.return_code, 0);

    // Initial world size

    auto fut2 = processes[1].invoke_async("world-check", "invocation-id", input_data.data(), input_data.size());
    auto ret2 = fut2.get();
    ASSERT_EQ(ret2.return_code, 0);

    for(auto & proc : processes) {
      ASSERT_TRUE(praas.stop_process(proc));
    }
  }
}

TEST_F(ScalingApplication, InitialWorldDef)
{
  praas::common::http::HTTPClientFactory::initialize(1);
  {
    praas::sdk::PraaS praas{fmt::format("http://127.0.0.1:{}", 9000)};

    std::string app_name("test_scaling2");
    ASSERT_TRUE(
      praas.create_application(app_name, "spcleth/praas:proc-local")
    );

    int procs = 2;
    std::vector<praas::sdk::Process> processes;
    std::vector<std::future<std::optional<praas::sdk::Process>>> futures;

    for(int i = 0; i < procs; ++i) {
      futures.emplace_back(praas.create_process_async(app_name, std::to_string(i), "1", "1024"));
    }

    for(auto & fut : futures) {
      auto val = fut.get();
      ASSERT_TRUE(val.has_value());
      processes.push_back(std::move(val.value()));
      ASSERT_TRUE(processes.back().connect());
    }

    for(int i = 0; i < procs; ++i) {
      ASSERT_TRUE(processes[i].connect());
    }

    praas::sdk::Process::pool.configure(8);

    Result msg;
    msg.active.emplace_back(std::to_string(0));
    msg.active.emplace_back(std::to_string(1));
    auto input_data = get_input_binary(msg);

    // Notification that the second process was created.

    auto fut = processes[0].invoke_async("world-check", "invocation-id", input_data.data(), input_data.size());
    auto ret = fut.get();
    ASSERT_EQ(ret.return_code, 0);

    // Initial world size

    auto fut2 = processes[1].invoke_async("world-check", "invocation-id", input_data.data(), input_data.size());
    auto ret2 = fut2.get();
    ASSERT_EQ(ret2.return_code, 0);

    for(auto & proc : processes) {
      ASSERT_TRUE(praas.stop_process(proc));
    }
  }
}

TEST_F(ScalingApplication, SwappingUpdate)
{
  praas::common::http::HTTPClientFactory::initialize(1);
  {
    praas::sdk::PraaS praas{fmt::format("http://127.0.0.1:{}", 9000)};

    std::string app_name("test_scaling3");
    ASSERT_TRUE(
      praas.create_application(app_name, "spcleth/praas:proc-local")
    );

    int procs = 2;
    std::vector<praas::sdk::Process> processes;
    std::vector<std::future<std::optional<praas::sdk::Process>>> futures;

    for(int i = 0; i < procs; ++i) {
      futures.emplace_back(praas.create_process_async(app_name, std::to_string(i), "1", "1024"));
    }

    for(auto & fut : futures) {
      auto val = fut.get();
      ASSERT_TRUE(val.has_value());
      processes.push_back(std::move(val.value()));
      ASSERT_TRUE(processes.back().connect());
    }

    for(int i = 0; i < procs; ++i) {
      ASSERT_TRUE(processes[i].connect());
    }

    praas::sdk::Process::pool.configure(4);

    {
      spdlog::info("Scenario 1: P0 knows about swapping of P1");

      auto [swap_res, swap_msg] = praas.swap_process(processes[1]);
      ASSERT_TRUE(swap_res);

      Result msg_scenario1;
      msg_scenario1.active.emplace_back(std::to_string(0));
      msg_scenario1.swapped.emplace_back(std::to_string(1));
      auto input_data = get_input_binary(msg_scenario1);

      auto ret = processes[0].invoke("world-check", "invocation-id", input_data.data(), input_data.size());
      ASSERT_EQ(ret.return_code, 0);
    }

    spdlog::info("Scenario 2: P2 joins and knows about P0 and swapping of P1");
    {
      auto proc_status = praas.create_process(app_name, "2", "1", "1024");
      ASSERT_TRUE(proc_status.has_value());
      processes.push_back(std::move(proc_status.value()));
      ASSERT_TRUE(processes.back().connect());

      Result msg_scenario;
      msg_scenario.active.emplace_back(std::to_string(0));
      msg_scenario.active.emplace_back(std::to_string(2));
      msg_scenario.swapped.emplace_back(std::to_string(1));
      auto input_data = get_input_binary(msg_scenario);

      auto ret = processes[0].invoke("world-check", "invocation-id", input_data.data(), input_data.size());
      ASSERT_EQ(ret.return_code, 0);

      ret = processes[2].invoke("world-check", "invocation-id", input_data.data(), input_data.size());
      ASSERT_EQ(ret.return_code, 0);
    }

    spdlog::info("Scenario 3: P1 comes back and knows about P0 and P1. P0 and P2 know P3 are active");
    {
      auto swapped_proc = praas.swapin_process(app_name, processes[1].process_id);
      ASSERT_TRUE(swapped_proc.has_value());
      processes[1] = std::move(swapped_proc.value());
      processes[1].connect();

      Result msg_scenario;
      msg_scenario.active.emplace_back(std::to_string(0));
      msg_scenario.active.emplace_back(std::to_string(2));
      msg_scenario.active.emplace_back(std::to_string(1));
      auto input_data = get_input_binary(msg_scenario);

      std::vector<std::future<praas::sdk::InvocationResult>> futures;
      for(auto & proc : processes) {
        futures.emplace_back(
          proc.invoke_async("world-check", "invocation-id", input_data.data(), input_data.size())
        );
      }

      for(auto & fut : futures) {
        auto ret = fut.get();
        ASSERT_EQ(ret.return_code, 0);
      }
    }
  }
}

TEST_F(ScalingApplication, ClosingProcess)
{
  praas::common::http::HTTPClientFactory::initialize(1);
  {
    praas::sdk::PraaS praas{fmt::format("http://127.0.0.1:{}", 9000)};

    std::string app_name("test_scaling4");
    ASSERT_TRUE(
      praas.create_application(app_name, "spcleth/praas:proc-local")
    );

    int procs = 2;
    std::vector<praas::sdk::Process> processes;
    std::vector<std::future<std::optional<praas::sdk::Process>>> futures;

    for(int i = 0; i < procs; ++i) {
      futures.emplace_back(praas.create_process_async(app_name, std::to_string(i), "1", "1024"));
    }

    for(auto & fut : futures) {
      auto val = fut.get();
      ASSERT_TRUE(val.has_value());
      processes.push_back(std::move(val.value()));
      ASSERT_TRUE(processes.back().connect());
    }

    for(int i = 0; i < procs; ++i) {
      ASSERT_TRUE(processes[i].connect());
    }

    praas::sdk::Process::pool.configure(4);

    spdlog::info("Scenario 1: P0 is closed, P1 should know about this.");
    {

      ASSERT_TRUE(praas.stop_process(processes[0]));

      Result msg_scenario1;
      msg_scenario1.active.emplace_back(std::to_string(1));
      auto input_data = get_input_binary(msg_scenario1);

      auto ret = processes[1].invoke("world-check", "invocation-id", input_data.data(), input_data.size());
      ASSERT_EQ(ret.return_code, 0);
    }

  }
}

TEST_F(ScalingApplication, DeletingProcess)
{
  praas::common::http::HTTPClientFactory::initialize(1);
  {
    praas::sdk::PraaS praas{fmt::format("http://127.0.0.1:{}", 9000)};

    std::string app_name("test_scaling5");
    ASSERT_TRUE(
      praas.create_application(app_name, "spcleth/praas:proc-local")
    );

    int procs = 2;
    std::vector<praas::sdk::Process> processes;
    std::vector<std::future<std::optional<praas::sdk::Process>>> futures;

    for(int i = 0; i < procs; ++i) {
      futures.emplace_back(praas.create_process_async(app_name, std::to_string(i), "1", "1024"));
    }

    for(auto & fut : futures) {
      auto val = fut.get();
      ASSERT_TRUE(val.has_value());
      processes.push_back(std::move(val.value()));
      ASSERT_TRUE(processes.back().connect());
    }

    for(int i = 0; i < procs; ++i) {
      ASSERT_TRUE(processes[i].connect());
    }

    praas::sdk::Process::pool.configure(4);

    spdlog::info("Scenario 1: P0 is swapped and delete, P1 should know about both changes.");
    {
      auto [swap_res, swap_msg] = praas.swap_process(processes[0]);
      ASSERT_TRUE(swap_res);

      Result msg_scenario1;
      msg_scenario1.active.emplace_back(std::to_string(1));
      msg_scenario1.swapped.emplace_back(std::to_string(0));
      auto input_data = get_input_binary(msg_scenario1);

      auto ret = processes[1].invoke("world-check", "invocation-id", input_data.data(), input_data.size());
      ASSERT_EQ(ret.return_code, 0);

      ASSERT_TRUE(praas.delete_process(processes[0]));
      msg_scenario1.swapped.clear();
      input_data = get_input_binary(msg_scenario1);

      ret = processes[1].invoke("world-check", "invocation-id", input_data.data(), input_data.size());
      ASSERT_EQ(ret.return_code, 0);
    }

  }
}

