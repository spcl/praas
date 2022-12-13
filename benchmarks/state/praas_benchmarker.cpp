#include <praas/sdk/process.hpp>
#include <praas/common/messages.hpp>
#include <praas/common/application.hpp>

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
  std::vector<int> messages;
  int repetitions;

  std::string process_address;
  int process_port;

  std::string function_name;

  std::string output_file;

  template<typename Ar>
  void serialize(Ar & ar)
  {
    ar(CEREAL_NVP(sizes));
    ar(CEREAL_NVP(messages));
    ar(CEREAL_NVP(repetitions));

    ar(CEREAL_NVP(process_address));
    ar(CEREAL_NVP(process_port));
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

  praas::sdk::Process proc{cfg.process_address, cfg.process_port};

  spdlog::error("Connecting to {}:{}", cfg.process_address, cfg.process_port);
  if(!proc.connect())  {
    spdlog::error("Could not connect to {}:{}", cfg.process_address, cfg.process_port);
    return 1;
  }

  std::vector< std::vector<long> > measurements;
  auto result = proc.invoke("aws_sdk_init", "id", nullptr, 0);

  for(int size : cfg.sizes) {

    spdlog::info("Begin size {}", size);

    Invocations in;
    in.bucket = "praas-communication";
    in.redis_hostname = "";
    in.data.resize(size);
    for(int i = 0; i < size; ++i)
      in.data[i] = i + 1;
    std::stringstream str_receiver;
    {
      cereal::BinaryOutputArchive archive_out(str_receiver);
      in.save(archive_out);
    }
    std::string input_receiver = str_receiver.str();

    for(int msg : cfg.messages) {

      spdlog::info("Begin message {}", msg);

      measurements.emplace_back();
      for(int i = 0; i  < cfg.repetitions + 1; ++i) {

        auto begin = std::chrono::high_resolution_clock::now();
        auto result = proc.invoke(cfg.function_name, "id", input_receiver.data(), input_receiver.length());
        auto end = std::chrono::high_resolution_clock::now();

        if(i > 0)
          measurements.back().emplace_back(
            std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count()
          );

      }
    }

  }

  std::ofstream out_file{cfg.output_file, std::ios::out};
  out_file << "size,msg,repetition,time" << '\n';
  int i = 0;
  for(int size : cfg.sizes) {
    for(int msg : cfg.messages) {

      for(int j = 0; j < cfg.repetitions; ++j) {
        out_file << size << "," << msg << "," << j << "," << measurements[i][j] << '\n';
      }
      ++i;
    }
  }
  out_file.close();

  proc.disconnect();

  return 0;
}
