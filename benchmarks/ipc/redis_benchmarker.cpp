#include <praas/sdk/process.hpp>

#include <cereal/archives/binary.hpp>
#include <fstream>

#include <boost/iostreams/stream.hpp>
#include <boost/interprocess/streams/bufferstream.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/details/helpers.hpp>
#include <spdlog/spdlog.h>
#include <thread>

#include "functions/cpp/types.hpp"

struct Config
{
  std::vector<int> sizes;
  int repetitions;

  std::string process_address_sender;
  int process_port_sender;
  std::string process_address_receiver;
  int process_port_receiver;
  std::string redis_hostname;

  std::string function_name;

  std::string output_file;

  template<typename Ar>
  void serialize(Ar & ar)
  {
    ar(CEREAL_NVP(sizes));
    ar(CEREAL_NVP(repetitions));

    ar(CEREAL_NVP(process_address_sender));
    ar(CEREAL_NVP(process_port_sender));
    ar(CEREAL_NVP(process_address_receiver));
    ar(CEREAL_NVP(process_port_receiver));
    ar(CEREAL_NVP(redis_hostname));
    ar(CEREAL_NVP(function_name));

    ar(CEREAL_NVP(output_file));
  }

};

int main(int argc, char** argv)
{
  std::string config_file{argv[1]};
  std::ifstream in_stream{config_file};
  if (!in_stream.is_open()) {
    spdlog::error("Could not open config file {}", config_file);
    exit(1);
  }

  Config cfg;
  cereal::JSONInputArchive archive_in(in_stream);
  cfg.serialize(archive_in);

  spdlog::set_pattern("[%H:%M:%S:%f] [P %P] [T %t] [%l] %v ");
  spdlog::info("Executing PraaS benchmarker!");

  praas::sdk::Process proc_sender{cfg.process_address_sender, cfg.process_port_sender};
  praas::sdk::Process proc_receiver{cfg.process_address_receiver, cfg.process_port_receiver};

  spdlog::error("Connecting to {}:{}", cfg.process_address_sender, cfg.process_port_sender);
  if(!proc_sender.connect())  {
    spdlog::error("Could not connect to {}:{}", cfg.process_address_sender, cfg.process_port_sender);
    return 1;
  }
  spdlog::error("Connecting to {}:{}", cfg.process_address_receiver, cfg.process_port_receiver);
  if(!proc_receiver.connect())  {
    spdlog::error("Could not connect to {}:{}", cfg.process_address_receiver, cfg.process_port_receiver);
    return 1;
  }

  Invocations in_sender{};
  in_sender.repetitions = cfg.repetitions;
  in_sender.sizes = cfg.sizes;
  in_sender.sender = true;
  in_sender.redis_hostname = cfg.redis_hostname;
  std::stringstream str_sender;
  {
    cereal::BinaryOutputArchive archive_out(str_sender);
    in_sender.save(archive_out);
  }
  std::string input_sender = str_sender.str();

  Invocations in_receiver{};
  in_receiver.repetitions = cfg.repetitions;
  in_receiver.sizes = cfg.sizes;
  in_receiver.sender = false;
  in_receiver.redis_hostname = cfg.redis_hostname;
  std::stringstream str_receiver;
  {
    cereal::BinaryOutputArchive archive_out(str_receiver);
    in_receiver.save(archive_out);
  }
  std::string input_receiver = str_receiver.str();

  praas::sdk::InvocationResult sender;
  praas::sdk::InvocationResult receiver;

  std::thread receiver_thread{
    [&]() {
      receiver = proc_receiver.invoke(cfg.function_name, "id", input_receiver.data(), input_receiver.length());
    }
  };
  sender = proc_sender.invoke(cfg.function_name, "id", input_sender.data(), input_sender.length());
  receiver_thread.join();

  Results res_rcv, res_sender;
  {
    boost::iostreams::stream<boost::iostreams::array_source> stream(
      receiver.payload.get(), receiver.payload_len
    );
    cereal::BinaryInputArchive archive_out(stream);
    res_rcv.load(archive_out);
  }
  {
    boost::iostreams::stream<boost::iostreams::array_source> stream(
      sender.payload.get(), sender.payload_len
    );
    cereal::BinaryInputArchive archive_out(stream);
    res_sender.load(archive_out);
  }

  std::ofstream out_file{cfg.output_file + "_sender", std::ios::out};
  out_file << "size,repetition,time,poll_time" << '\n';
  for(int i = 0; i < res_sender.measurements.size(); ++i) {

    for(int j = 0; j < cfg.repetitions; ++j) {
      out_file << cfg.sizes[i] << "," << j << "," << std::get<0>(res_sender.measurements[i][j]) << " " << std::get<1>(res_sender.measurements[i][j]) << '\n';
    }

  }
  out_file.close();

  std::ofstream out_file2{cfg.output_file + "_receiver", std::ios::out};
  out_file2 << "size,repetition,time,poll_time" << '\n';
  for(int i = 0; i < res_rcv.measurements.size(); ++i) {

    for(int j = 0; j < cfg.repetitions; ++j) {
      out_file2 << cfg.sizes[i] << "," << j << "," << std::get<0>(res_rcv.measurements[i][j]) << " " << std::get<1>(res_rcv.measurements[i][j]) << '\n';
    }

  }
  out_file2.close();

  proc_sender.disconnect();
  proc_receiver.disconnect();

  return 0;
}
