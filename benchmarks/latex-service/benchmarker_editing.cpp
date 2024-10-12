#include <praas/common/application.hpp>
#include <praas/common/messages.hpp>
#include <praas/sdk/praas.hpp>
#include <praas/sdk/process.hpp>

#include <filesystem>
#include <fstream>

#include <boost/interprocess/streams/bufferstream.hpp>
#include <boost/iostreams/stream.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/details/helpers.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <spdlog/spdlog.h>
#include <thread>

#include "Base64.h"

struct Config {
  std::string inputs_directory;
  std::vector<std::string> input_files;
  int repetitions;

  std::string resources_cpu;
  std::string resources_memory;

  std::string control_plane_address;

  std::string output_file;
  std::string cloud_resource_name;

  template <typename Ar>
  void serialize(Ar& ar)
  {
    ar(CEREAL_NVP(inputs_directory));
    ar(CEREAL_NVP(input_files));
    ar(CEREAL_NVP(repetitions));
    ar(CEREAL_NVP(resources_cpu));
    ar(CEREAL_NVP(resources_memory));

    ar(CEREAL_NVP(control_plane_address));
    ar(CEREAL_NVP(output_file));
    ar(CEREAL_NVP(cloud_resource_name));
  }
};

struct File {

  std::string path;
  std::string file;
  std::string data;

  template <typename Ar>
  void save(Ar& archive) const
  {
    archive(CEREAL_NVP(path));
    archive(CEREAL_NVP(file));
    archive(CEREAL_NVP(data));
  }

  template <typename Ar>
  void load(Ar& archive)
  {
    archive(CEREAL_NVP(path));
    archive(CEREAL_NVP(file));
    archive(CEREAL_NVP(data));
  }
};

struct Result {
  std::string message;

  template <typename Ar>
  void save(Ar& archive) const
  {
    archive(CEREAL_NVP(message));
  }

  template <typename Ar>
  void load(Ar& archive)
  {
    archive(CEREAL_NVP(message));
  }
};

std::string generate_input_json(File& file)
{
  std::stringstream output;
  {
    cereal::JSONOutputArchive archive_out{output};
    file.save(archive_out);
    assert(stream.good());
  }
  return output.str();
}

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

  praas::sdk::PraaS praas{fmt::format("http://{}:{}", cfg.control_plane_address, 8000)};

  praas.create_application("test-latex-editing", cfg.cloud_resource_name);

  auto proc = praas.create_process(
      "test-latex-editing", "alloc_invoc_process", cfg.resources_cpu, cfg.resources_memory
  );
  if (!proc.has_value() || !proc->connect()) {
    abort();
  }

  std::vector<std::tuple<std::string, std::string, int, int, int, long>> measurements;

  for (std::string file : cfg.input_files) {

    spdlog::info("Begin file {}", file);

    File input_file;
    input_file.file = std::filesystem::path{file}.filename();
    input_file.path = std::filesystem::path{file}.parent_path();

    std::ifstream input{std::filesystem::path{cfg.inputs_directory} / file};
    std::stringstream buffer;
    buffer << input.rdbuf();
    input_file.data = buffer.str();

    if (std::filesystem::path{file}.extension() == ".pdf" ||
        std::filesystem::path{file}.extension() == ".png") {
      input_file.data = macaron::Base64::Encode(input_file.data);
    }
    auto data = generate_input_json(input_file);

    for (int i = 0; i < cfg.repetitions + 1; ++i) {

      auto begin = std::chrono::high_resolution_clock::now();
      auto invoc = proc->invoke("update-file", "invocation-id", data.data(), data.size());
      auto end = std::chrono::high_resolution_clock::now();
      if (invoc.return_code != 0) {
        abort();
      }
      if (i > 0) {
        measurements.emplace_back(
            "update-file", file, i, data.length(), invoc.payload_len,
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()
        );
      }
    }
  }

  for (std::string file : cfg.input_files) {

    spdlog::info("Begin file {}", file);

    File input_file;
    input_file.file = std::filesystem::path{file}.filename();
    input_file.path = std::filesystem::path{file}.parent_path();
    input_file.data = "";
    auto data = generate_input_json(input_file);

    for (int i = 0; i < cfg.repetitions + 1; ++i) {

      auto begin = std::chrono::high_resolution_clock::now();
      auto invoc = proc->invoke("get-file", "invocation-id", data.data(), data.size());
      auto end = std::chrono::high_resolution_clock::now();
      if (invoc.return_code != 0) {
        std::cerr << invoc.return_code << " " << file << " " << i << std::endl;
        abort();
      }
      if (i > 0) {
        measurements.emplace_back(
            "get-file", file, i, data.length(), invoc.payload_len,
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()
        );
      }
    }
  }
  std::cerr << measurements.size() << std::endl;

  std::ofstream out_file{cfg.output_file, std::ios::out};
  out_file << "type,input,repetition,input-size,output-size,time" << '\n';

  for (auto& [type, input, rep, input_size, output, time] : measurements) {
    out_file << type << "," << input << "," << rep << "," << input_size << "," << output << ","
             << time << '\n';
  }

  out_file.close();

  return 0;
}
