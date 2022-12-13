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
  std::vector<int> threads;
  int repetitions;

  std::string process_address;
  int process_port;

  std::string function_name;

  std::string output_file;

  template<typename Ar>
  void serialize(Ar & ar)
  {
    ar(CEREAL_NVP(sizes));
    ar(CEREAL_NVP(threads));
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
  //if(cfg.function_name == "s3_sender")
  auto result = proc.invoke("aws_sdk_init", "id", nullptr, 0);

  std::ofstream out_file{cfg.output_file, std::ios::out};
  out_file << "size,repetition,threads,time" << '\n';
  for(int threads : cfg.threads) {

    spdlog::info("Begin threads {}", threads);

    Invocations in;
    in.bucket = "praas-communication";
    //in.redis_hostname = cfg.redis_hostname;
    in.sizes = cfg.sizes;
    in.repetitions = cfg.repetitions;
    in.threads = threads;
    std::stringstream str_receiver;
    {
      cereal::BinaryOutputArchive archive_out(str_receiver);
      in.save(archive_out);
    }
    std::string input_receiver = str_receiver.str();

    Results res;
    auto result = proc.invoke(cfg.function_name, "id", input_receiver.data(), input_receiver.length());
    if(result.return_code != 0) {
      abort();
    }
    {
      boost::iostreams::stream<boost::iostreams::array_source> stream(
        result.payload.get(), result.payload_len
      );
      cereal::BinaryInputArchive archive_out(stream);
      res.load(archive_out);
    }

    for(int i = 0; i < res.measurements.size(); ++i) {

      if(i == 0)
        continue;

      for(int j = 0; j < cfg.repetitions; ++j) {
        std::cerr << j<<  " " << res.measurements[i].size() << std::endl;
        out_file << cfg.sizes[i] << "," << j << "," << threads << "," << res.measurements[i][j]
          << std::endl;
      }

    }
  }
  out_file.close();

  proc.disconnect();

  return 0;
}
