#include <praas/sdk/process.hpp>

#include <cereal/archives/binary.hpp>
#include <fstream>

#include <boost/iostreams/stream.hpp>
#include <boost/interprocess/streams/bufferstream.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/details/helpers.hpp>
#include <spdlog/spdlog.h>

#include "functions/cpp/types.hpp"

struct Config
{
  std::vector<int> sizes;
  int repetitions;

  std::string process_address;
  int process_port;

  std::string function_name;

  std::string output_file;

  template<typename Ar>
  void serialize(Ar & ar)
  {
    ar(CEREAL_NVP(sizes));
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

  Invocations in;
  in.repetitions = cfg.repetitions;
  in.sizes = cfg.sizes;
  std::stringstream str;
  {
    cereal::BinaryOutputArchive archive_out(str);
    in.save(archive_out);
  }
  std::string input = str.str();

  auto result = proc.invoke(cfg.function_name, "id", input.data(), input.length());
  boost::iostreams::stream<boost::iostreams::array_source> stream(
    result.payload.get(), result.payload_len
  );
  cereal::BinaryInputArchive archive_out(stream);
  Results res;
  res.load(archive_out);

  std::ofstream out_file{cfg.output_file, std::ios::out};
  out_file << "size, repetition, time" << '\n';
  for(int i = 0; i < res.measurements.size(); ++i) {

    for(int j = 0; j < cfg.repetitions; ++j) {
      out_file << cfg.sizes[i] << "," << j << "," << res.measurements[i][j] << '\n';
    }

  }
  out_file.close();

  proc.disconnect();

  return 0;
}
