
#include <cxxopts.hpp>
#include <spdlog/spdlog.h>

#include "server.hpp"

namespace praas::control_plane {

  Options opts(int argc, char ** argv)
  {
    cxxopts::Options options("rfaas-executor-manager", "Handle client connections and allocation of executors.");
    options.add_options()
      ("a,address", "IP address used by this server.",  cxxopts::value<std::string>())
      ("p,port", "TCP port to listen on.",  cxxopts::value<int>()->default_value("8080"))
      ("r,redis", "Redis address to use.",  cxxopts::value<std::string>())
      ("t,threads", "Number of processing threads.",  cxxopts::value<int>()->default_value("1"))
      ("backend", "Backend allocating functions. Options: [local, aws].",  cxxopts::value<std::string>())
      ("local-server", "Address of the local server.",  cxxopts::value<std::string>())
      ("read-timeout", "Read timeout for TCP sockets [ms].",  cxxopts::value<int>()->default_value("1000"))
      ("https-port", "HTTPS port to listen on.",  cxxopts::value<int>()->default_value("8050"))
      ("ssl-server-cert", "Path to server certificate file.",  cxxopts::value<std::string>())
      ("ssl-server-key", "Path to server key file.",  cxxopts::value<std::string>())
      ("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"))
    ;
    auto parsed_options = options.parse(argc, argv);

    Options result;
    result.ip_address = parsed_options["address"].as<std::string>();
    result.port = parsed_options["port"].as<int>();
    result.threads = parsed_options["threads"].as<int>();
    result.redis_addr = parsed_options["redis"].as<std::string>();
    result.https_port = parsed_options["https-port"].as<int>();
    result.ssl_server_cert = parsed_options["ssl-server-cert"].as<std::string>();
    result.ssl_server_key = parsed_options["ssl-server-key"].as<std::string>();
    result.verbose = parsed_options["verbose"].as<bool>();
    result.read_timeout = parsed_options["read-timeout"].as<int>();
    std::string backend = parsed_options["backend"].as<std::string>();
    if(backend == "local") {
      result.backend = FunctionBackendType::LOCAL;
      result.local_server = parsed_options["local-server"].as<std::string>();
    } else if(backend == "aws")
      result.backend = FunctionBackendType::AWS;
    else {
      spdlog::error("Incorrect choice of function backend {}", backend);
      exit(1);
    }

    return result;
  }


}
