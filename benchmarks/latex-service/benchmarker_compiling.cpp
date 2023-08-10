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

struct CompileRequest {
  std::string file;
  bool clean;

  template <typename Ar>
  void save(Ar& archive) const
  {
    archive(CEREAL_NVP(file));
    archive(CEREAL_NVP(clean));
  }

  template <typename Ar>
  void load(Ar& archive)
  {
    archive(CEREAL_NVP(clean));
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

struct CompilationResult {
  std::string status;
  std::string output;
  double time_data, time_compile;

  template <typename Ar>
  void save(Ar& archive) const
  {
    archive(CEREAL_NVP(status));
    archive(CEREAL_NVP(output));
    archive(CEREAL_NVP(time_data));
    archive(CEREAL_NVP(time_compile));
  }

  template <typename Ar>
  void load(Ar& archive)
  {
    archive(CEREAL_NVP(status));
    archive(CEREAL_NVP(output));
    archive(CEREAL_NVP(time_data));
    archive(CEREAL_NVP(time_compile));
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

struct ResultPDF {
  std::string status;
  std::string data;

  template <typename Ar>
  void save(Ar& archive) const
  {
    archive(CEREAL_NVP(status));
    archive(CEREAL_NVP(data));
  }

  template <typename Ar>
  void load(Ar& archive)
  {
    archive(CEREAL_NVP(status));
    archive(CEREAL_NVP(data));
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

std::string generate_input_compile(CompileRequest& req)
{
  std::stringstream output;
  {
    cereal::JSONOutputArchive archive_out{output};
    req.save(archive_out);
    assert(stream.good());
  }
  return output.str();
}

template <typename Res>
Res get_output(const char* buffer, size_t size)
{
  Res out;
  boost::iostreams::stream<boost::iostreams::array_source> stream(buffer, size);
  cereal::JSONInputArchive archive_in{stream};
  out.load(archive_in);

  return out;
}

std::string upload_file(std::string file, Config& cfg, bool read_data = true)
{
  File input_file;
  input_file.file = std::filesystem::path{file}.filename();
  input_file.path = std::filesystem::path{file}.parent_path();

  if (read_data) {
    std::ifstream input{std::filesystem::path{cfg.inputs_directory} / file};
    std::stringstream buffer;
    buffer << input.rdbuf();
    input_file.data = buffer.str();

    if (std::filesystem::path{file}.extension() == ".pdf" ||
        std::filesystem::path{file}.extension() == ".png") {
      input_file.data = macaron::Base64::Encode(input_file.data);
    }
  } else {
    input_file.data = "";
  }

  return generate_input_json(input_file);
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

  std::vector<std::string> files{
      "acmart.bib",       "acmart.dtx",          "ACM-Reference-Format.bst",
      "sample-base.bib",  "sample-sigconf.tex",  "sampleteaser.pdf",
      "acmart.cls",       "acmart.ins",          "sample-franklin.png",
      "secs/acks.tex",    "secs/appendices.tex", "secs/appendix.tex",
      "secs/authors.tex", "secs/citations.tex",  "secs/core.tex",
      "secs/figure.tex",  "secs/multi.tex",      "secs/sigchi.tex"};
  std::vector<std::tuple<std::string, std::string, int, int, int, long, double, double>>
      measurements;

  // Scenario 1 - full recompilation
  for (int i = 0; i < cfg.repetitions + 1; ++i) {

    spdlog::info("Begin rep {}", i);

    for (const auto& file : files) {
      auto data = upload_file(file, cfg);

      auto invoc = proc->invoke("update-file", "invocation-id", data.data(), data.size());
      if (invoc.return_code != 0) {
        abort();
      }
    }

    CompileRequest req{"sample-sigconf.tex", true};
    auto data = generate_input_compile(req);
    auto begin = std::chrono::high_resolution_clock::now();
    auto invoc = proc->invoke("compile", "invocation-id", data.data(), data.size());
    auto end = std::chrono::high_resolution_clock::now();
    if (invoc.return_code != 0) {
      abort();
    }
    auto res = get_output<CompilationResult>(invoc.payload.get(), invoc.payload_len);

    if (i > 0) {
      measurements.emplace_back(
          "compile", "full", i, data.length(), invoc.payload_len,
          std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count(), res.time_data,
          res.time_compile
      );
    }
  }

  std::vector<std::string> update_files{"secs/acks.tex"};
  // Scenario 1 - small update
  for (int i = 0; i < cfg.repetitions + 1; ++i) {

    spdlog::info("Begin rep {}", i);

    for (const auto& file : update_files) {
      auto data = upload_file(file, cfg, false);

      auto invoc = proc->invoke("update-file", "invocation-id", data.data(), data.size());
      if (invoc.return_code != 0) {
        abort();
      }
    }

    CompileRequest req{"sample-sigconf.tex", false};
    auto data = generate_input_compile(req);
    auto begin = std::chrono::high_resolution_clock::now();
    auto invoc = proc->invoke("compile", "invocation-id", data.data(), data.size());
    auto end = std::chrono::high_resolution_clock::now();
    if (invoc.return_code != 0) {
      abort();
    }
    auto res = get_output<CompilationResult>(invoc.payload.get(), invoc.payload_len);

    if (i > 0) {
      measurements.emplace_back(
          "compile", "small-update", i, data.length(), invoc.payload_len,
          std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count(), res.time_data,
          res.time_compile
      );
    }
  }

  update_files = {
      "acmart.cls", "sampleteaser.pdf", "secs/core.tex", "secs/figure.tex", "secs/multi.tex"};
  // Scenario 1 - large update
  for (int i = 0; i < cfg.repetitions + 1; ++i) {

    spdlog::info("Begin rep {}", i);

    for (const auto& file : update_files) {
      auto data = upload_file(file, cfg, false);

      auto invoc = proc->invoke("update-file", "invocation-id", data.data(), data.size());
      if (invoc.return_code != 0) {
        abort();
      }
    }

    CompileRequest req{"sample-sigconf.tex", false};
    auto data = generate_input_compile(req);
    auto begin = std::chrono::high_resolution_clock::now();
    auto invoc = proc->invoke("compile", "invocation-id", data.data(), data.size());
    auto end = std::chrono::high_resolution_clock::now();
    if (invoc.return_code != 0) {
      abort();
    }
    auto res = get_output<CompilationResult>(invoc.payload.get(), invoc.payload_len);

    if (i > 0) {
      measurements.emplace_back(
          "compile", "large-update", i, data.length(), invoc.payload_len,
          std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count(), res.time_data,
          res.time_compile
      );
    }
  }

  for (int i = 0; i < cfg.repetitions + 1; ++i) {

    spdlog::info("Begin rep {}", i);
    CompileRequest req{"sample-sigconf", false};
    auto data = generate_input_compile(req);
    auto begin = std::chrono::high_resolution_clock::now();
    auto invoc = proc->invoke("get-pdf", "invocation-id", data.data(), data.size());
    auto end = std::chrono::high_resolution_clock::now();
    if (invoc.return_code != 0) {
      abort();
    }
    auto res = get_output<ResultPDF>(invoc.payload.get(), invoc.payload_len);

    if (i > 0) {
      measurements.emplace_back(
          "get-pdf", "default", i, data.length(), invoc.payload_len,
          std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count(), 0, 0
      );
    }
  }

  std::cerr << measurements.size() << std::endl;

  std::ofstream out_file{cfg.output_file, std::ios::out};
  out_file << "type,input,repetition,input-size,output-size,time,download-time,compile-time"
           << '\n';

  for (auto& [type, input, rep, input_size, output, time, download_time, compile_time] :
       measurements) {
    out_file << type << "," << input << "," << rep << "," << input_size << ",";
    out_file << output << "," << time << "," << download_time << "," << compile_time << '\n';
  }

  out_file.close();

  return 0;
}
