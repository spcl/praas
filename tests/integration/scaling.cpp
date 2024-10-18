
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

//std::string get_output_binary(const char* buffer, size_t size)
//{
//  Result out;
//  boost::iostreams::stream<boost::iostreams::array_source> stream(buffer, size);
//  cereal::BinaryInputArchive archive_in{stream};
//  out.load(archive_in);
//
//  return out.msg;
//}

std::string get_input_binary(Result& msg)
{
  //result.resize(msg.active.size() + msg.swapped.size() * 2);
  //jjjstd::string result;

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


//TEST_F(ScalingApplication, AutomaticSwap)
//{
//  praas::common::http::HTTPClientFactory::initialize(1);
//  {
//    praas::sdk::PraaS praas{fmt::format("http://127.0.0.1:{}", 9000)};
//
//    std::string app_name("test_scaling");
//    ASSERT_TRUE(
//      praas.create_application(app_name, "spcleth/praas:proc-local")
//    );
//
//    int procs = 2;
//    std::vector<praas::sdk::Process> processes;
//    std::vector<std::future<std::optional<praas::sdk::Process>>> futures;
//
//    for(int i = 0; i < procs; ++i) {
//      futures.emplace_back(praas.create_process_async(app_name, std::to_string(i), "1", "1024"));
//    }
//
//    for(auto & fut : futures) {
//      auto val = fut.get();
//      ASSERT_TRUE(val.has_value());
//      processes.push_back(std::move(val.value()));
//      ASSERT_TRUE(processes.back().connect());
//    }
//
//    praas::sdk::Process::pool.configure(8);
//
//    std::vector<std::future<praas::sdk::InvocationResult>> invoc_futures;
//    for(int i = 0; i < procs; ++i) {
//      invoc_futures.push_back(
//        processes[i].invoke_async("world-check", "invocation-id", nullptr, 0)
//      );
//    }
//
//    for(auto & fut : invoc_futures) {
//      auto val = fut.get();
//      EXPECT_EQ(val.return_code, 0);
//    }
//
//    //for(int i = 0; i < procs; ++i) {
//    //  ASSERT_TRUE(praas.stop_process(processes[i]));
//    //}
//
//    //Result msg;
//    //msg.msg = "TEST_SWAP";
//    //auto res = get_input_binary(msg);
//
//    //auto invoc = proc->invoke("state-put", "invocation-id", res.data(), res.size());
//    //ASSERT_EQ(invoc.return_code, 0);
//
//    //auto [swap_res, swap_msg] = praas.swap_process(proc.value());
//    //ASSERT_TRUE(swap_res);
//
//    //auto new_proc = praas.swapin_process(app_name, "alloc_invoc_process");
//    //ASSERT_TRUE(new_proc.has_value());
//
//    //ASSERT_TRUE(new_proc->connect());
//    //invoc = new_proc->invoke("state-get", "invocation-id2", res.data(), res.size());
//    //ASSERT_EQ(invoc.return_code, 0);
//
//    //auto res2 = get_output_binary(invoc.payload.get(), invoc.payload_len);
//    //ASSERT_EQ(msg.msg, res2);
//
//    //std::tie(swap_res, swap_msg) = praas.swap_process(new_proc.value());
//    //ASSERT_TRUE(swap_res);
//
//    //ASSERT_TRUE(praas.delete_process(new_proc.value()));
//
//    //auto failed_proc = praas.swapin_process(app_name, "alloc_invoc_process");
//    //ASSERT_FALSE(failed_proc.has_value());
//
//    //ASSERT_TRUE(praas.delete_application(app_name));
//  }
//}
//
