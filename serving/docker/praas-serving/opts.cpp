
#include <cxxopts.hpp>

#include "server.hpp"

namespace praas::serving {

  Options opts(int argc, char** argv)
  {
    cxxopts::Options options(
        "praas-serving", "Handle client connections and allocation of executors."
    );
    options.add_options()
      ("hole-puncher-addr", "IP address of hole puncher.",  cxxopts::value<std::string>())
      ("p,port", "TCP port to listen on.",  cxxopts::value<int>()->default_value("8080"))
      ("processes", "Number of subprocesseses to support.", cxxopts::value<int>()->default_value("1"))
      ("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"))
      ("enable-session-swapping", "Enable session swapping.", cxxopts::value<bool>()->default_value("true"))
      ("use-docker", "Use Docker.", cxxopts::value<bool>()->default_value("true"))
    ;
    auto parsed_options = options.parse(argc, argv);

    Options result;
    result.port = parsed_options["port"].as<int>();
    result.processes = parsed_options["processes"].as<int>();
    result.verbose = parsed_options["verbose"].as<bool>();
    result.enable_swapping = parsed_options["enable-session-swapping"].as<bool>();
    result.use_docker = parsed_options["use-docker"].as<bool>();
    result.hole_puncher_address = parsed_options["hole-puncher-addr"].as<std::string>();

    return result;
  }

} // namespace praas::serving
