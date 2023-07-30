
#include <praas/serving/docker/server.hpp>

#include <cxxopts.hpp>

namespace praas::serving::docker {

  Options opts(int argc, char** argv)
  {
    cxxopts::Options options(
        "praas-serving", "Handle client connections and allocation of processes."
    );
    options.add_options()
      ("p,http-port", "HTTP port to listen on.",  cxxopts::value<int>()->default_value("8080"))
      ("d,docker-port", "Port at which Docker daemon is listening.",  cxxopts::value<int>())
      ("process-port", "Ports used by the dockerized process controller.",  cxxopts::value<int>()->default_value("8000"))
      ("max-processes", "Number of processes to support.", cxxopts::value<int>()->default_value("1"))
      ("threads", "Number of threads in the HTTP server.", cxxopts::value<int>()->default_value("1"))
      ("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"))
    ;
    auto parsed_options = options.parse(argc, argv);

    Options result;
    result.http_port = parsed_options["http-port"].as<int>();
    result.docker_port = parsed_options["docker-port"].as<int>();
    result.process_port = parsed_options["process-port"].as<int>();
    result.threads = parsed_options["threads"].as<int>();
    result.max_processes = parsed_options["max-processes"].as<int>();
    result.verbose = parsed_options["verbose"].as<bool>();

    return result;
  }

} // namespace praas::serving::docker
