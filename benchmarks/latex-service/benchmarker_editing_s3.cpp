#include <praas/common/application.hpp>
#include <praas/common/messages.hpp>
#include <praas/sdk/praas.hpp>
#include <praas/sdk/process.hpp>

#include <drogon/HttpResponse.h>
#include <drogon/HttpTypes.h>
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
#include "praas/common/http.hpp"

#include <curl/curl.h>

struct Config {
  std::string inputs_directory;
  std::vector<std::string> input_files;
  int repetitions;

  std::string lambda_url_update;
  std::string lambda_url_get;
  std::string resources_memory;

  std::string output_file;

  template <typename Ar>
  void serialize(Ar& ar)
  {
    ar(CEREAL_NVP(inputs_directory));
    ar(CEREAL_NVP(input_files));
    ar(CEREAL_NVP(repetitions));
    ar(CEREAL_NVP(lambda_url_update));
    ar(CEREAL_NVP(lambda_url_get));
    ar(CEREAL_NVP(resources_memory));

    ar(CEREAL_NVP(output_file));
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
CURL* curl = curl_easy_init();

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
  ((std::string*)userp)->append((char*)contents, size * nmemb);
  return size * nmemb;
}

std::tuple<int, double> make_request(std::string url, const std::string& data)
{
  std::string header[] = {"Content-Type: application/json"};
  if (curl) {
    int len;
    char* str = curl_easy_unescape(curl, url.c_str(), url.length(), &len);
    curl_easy_setopt(curl, CURLOPT_URL, str);
    std::string readBuffer;
    struct curl_slist* list = NULL;
    list = curl_slist_append(list, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    // curl_easy_setopt(curl, CURLOPT_USERPWD,
    // "23bc46b1-71f6-4ed5-8c54-816aa4f8c502:123zO3xZCLrMN6v2BKK1dXYFpXlPkccOFqm12CdAsMgRU4VrNZ9lyGVCGuMDGIwP");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    // curl_easy_setopt(curl, CURLOPT_WRITEDATA, stdout);
    readBuffer.clear();
    auto begin = std::chrono::high_resolution_clock::now();
    auto res = curl_easy_perform(curl);
    auto end = std::chrono::high_resolution_clock::now();
    curl_slist_free_all(list);
    if (res != CURLE_OK) {
      abort();
    }

    return std::make_tuple(
        readBuffer.length(),
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()
    );
  }
}

int main(int argc, char** argv)
{
  praas::common::http::HTTPClientFactory::initialize(2);

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

  auto client = praas::common::http::HTTPClientFactory::create_client(cfg.lambda_url_update);

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
    std::cerr << data << std::endl;

    for (int i = 0; i < cfg.repetitions + 1; ++i) {

      std::promise<int> p;
      auto [length, duration] = make_request(cfg.lambda_url_update, data);
      // auto begin = std::chrono::high_resolution_clock::now();
      // client.post(
      //     data,
      //     [&](drogon::ReqResult result, const drogon::HttpResponsePtr& response) mutable {
      //       // std::cerr << result << std::endl;
      //       // std::cerr << response->getStatusCode() << std::endl;
      //       // if (response->getJsonObject())
      //       //   std::cerr << *response->getJsonObject() << std::endl;
      //       if (result != drogon::ReqResult::Ok && response->getStatusCode() != drogon::k200OK)
      //         abort();
      //       p.set_value(response->getJsonObject()->size());
      //     }
      //);
      // int val = p.get_future().get();
      // auto end = std::chrono::high_resolution_clock::now();
      if (i > 0) {
        measurements.emplace_back("update-file", file, i, data.length(), length, duration);
      }
    }
  }

  client = praas::common::http::HTTPClientFactory::create_client(cfg.lambda_url_get);

  for (std::string file : cfg.input_files) {

    spdlog::info("Begin file {}", file);

    File input_file;
    input_file.file = std::filesystem::path{file}.filename();
    input_file.path = std::filesystem::path{file}.parent_path();
    input_file.data = "";
    auto data = generate_input_json(input_file);

    for (int i = 0; i < cfg.repetitions + 1; ++i) {

      // std::promise<int> p;
      auto [length, duration] = make_request(cfg.lambda_url_get, data);
      // auto begin = std::chrono::high_resolution_clock::now();
      // client.post(
      //     data,
      //     [&](drogon::ReqResult result, const drogon::HttpResponsePtr& response) mutable {
      //       // std::cerr << result << std::endl;
      //       // std::cerr << response->getStatusCode() << std::endl;
      //       // if (response->getJsonObject())
      //       //   std::cerr << *response->getJsonObject() << std::endl;
      //       if (result != drogon::ReqResult::Ok && response->getStatusCode() != drogon::k200OK)
      //         abort();
      //       p.set_value(response->getJsonObject()->size());
      //     }
      //);
      // int val = p.get_future().get();
      // auto end = std::chrono::high_resolution_clock::now();
      if (i > 0) {
        measurements.emplace_back(
            "get-file", file, i, data.length(), length, duration
            // val,
            // std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()
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
  curl_easy_cleanup(curl);

  return 0;
}
