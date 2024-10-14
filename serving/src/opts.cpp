
#include <praas/serving/docker/server.hpp>

#include <cereal/archives/json.hpp>
#include <cxxopts.hpp>

#include <fstream>

namespace praas::serving::docker {

  void Options::serialize(cereal::JSONInputArchive& archive)
  {
    archive(cereal::make_nvp("server-address", this->server_ip));
    archive(cereal::make_nvp("http-port", this->http_port));
    archive(cereal::make_nvp("docker-port", this->docker_port));
    archive(cereal::make_nvp("process-port", this->process_port));
    archive(cereal::make_nvp("threads", this->threads));
    archive(cereal::make_nvp("max-processes", this->max_processes));
    archive(cereal::make_nvp("swaps-volume", this->swaps_volume));
  }

  std::optional<Options> opts(int argc, char** argv)
  {
    cxxopts::Options options(
        "praas-serving", "Handle client connections and allocation of processes."
    );
    options.add_options()("c,config", "Config to load.", cxxopts::value<std::string>())(
        "v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false")
    );
    auto parsed_options = options.parse(argc, argv);

    std::string config_file{parsed_options["config"].as<std::string>()};
    std::ifstream in_stream{config_file};
    if (!in_stream.is_open()) {
      spdlog::error("Could not open config file {}", config_file);
      return std::nullopt;
    }

    Options result{};
    cereal::JSONInputArchive archive_in(in_stream);
    result.serialize(archive_in);
    return result;
  }

} // namespace praas::serving::docker
