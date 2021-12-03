
#include <cxxopts.hpp>
#include <spdlog/spdlog.h>

#include "server.hpp"

namespace praas::control_plane {

  Options opts(int argc, char ** argv)
  {
    cxxopts::Options options("rfaas-executor-manager", "Handle client connections and allocation of executors.");
    options.add_options()
      ("p,port", "TCP port to listen on.",  cxxopts::value<int>()->default_value("8080"))
      ("r,redis", "Redis address to use.",  cxxopts::value<std::string>())
      ("t,threads", "Number of processing threads.",  cxxopts::value<int>()->default_value("1"))
      ("backend", "Backend allocating functions. Options: [local, aws].",  cxxopts::value<std::string>())
      ("local-server", "Address of the local server",  cxxopts::value<std::string>())
      ("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"))
    ;
    auto parsed_options = options.parse(argc, argv);

    Options result;
    result.port = parsed_options["port"].as<int>();
    result.threads = parsed_options["threads"].as<int>();
    result.redis_addr = parsed_options["redis"].as<std::string>();
    result.verbose = parsed_options["verbose"].as<bool>();
    std::string backend = parsed_options["backend"].as<std::string>();
    if(backend == "local") {
      result.backend = FunctionBackend::LOCAL;
      result.local_server = parsed_options["local-server"].as<std::string>();
    } else if(backend == "aws")
      result.backend = FunctionBackend::AWS;
    else {
      spdlog::error("Incorrect choice of function backend {}", backend);
      exit(1);
    }

    return result;
  }


}
