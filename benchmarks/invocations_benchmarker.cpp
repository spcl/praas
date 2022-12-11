#include <praas/sdk/process.hpp>

#include <fstream>

#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/details/helpers.hpp>
#include <spdlog/spdlog.h>

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

  std::vector< std::vector<long> > measurements;

  for(int size : cfg.sizes) {

    spdlog::info("Begin size {}", size);

    std::unique_ptr<char[]> buf{new char[size]};
    for(int i = 0; i < size / 4; ++i)
      ((int*)buf.get())[i] = i + 1;

    measurements.emplace_back();
    for(int i = 0; i  < cfg.repetitions; ++i) {

      auto begin = std::chrono::high_resolution_clock::now();
      auto result = proc.invoke(cfg.function_name, "id", buf.get(), size);
      auto end = std::chrono::high_resolution_clock::now();

      measurements.back().emplace_back(
        std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count()
      );

    }

  }

  std::ofstream out_file{cfg.output_file, std::ios::out};
  out_file << "size, repetition, time" << '\n';
  for(int i = 0; i < measurements.size(); ++i) {

    for(int j = 0; j < cfg.repetitions; ++j) {
      out_file << cfg.sizes[i] << "," << j << "," << measurements[i][j] << '\n';
    }

  }
  out_file.close();

  proc.disconnect();

  return 0;
}
