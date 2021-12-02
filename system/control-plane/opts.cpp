
#include <cxxopts.hpp>

#include "server.hpp"

namespace praas::control_plane {

  Options opts(int argc, char ** argv)
  {
    cxxopts::Options options("rfaas-executor-manager", "Handle client connections and allocation of executors.");
    options.add_options()
      ("p,port", "TCP port to listen on.",  cxxopts::value<int>()->default_value("8080"))
      ("r,redis", "Redis address to use.",  cxxopts::value<std::string>())
      ("t,threads", "Number of processing threads.",  cxxopts::value<int>()->default_value("1"))
      ("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"))
    ;
    auto parsed_options = options.parse(argc, argv);

    Options result;
    result.port = parsed_options["port"].as<int>();
    result.threads = parsed_options["threads"].as<int>();
    result.redis_addr = parsed_options["redis"].as<std::string>();
    std::cerr << result.redis_addr << '\n';
    result.verbose = parsed_options["verbose"].as<bool>();

    return result;
  }


}
