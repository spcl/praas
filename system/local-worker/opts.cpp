
#include <cxxopts.hpp>

#include "server.hpp"

namespace praas::local_worker {

  Options opts(int argc, char ** argv)
  {
    cxxopts::Options options("rfaas-executor-manager", "Handle client connections and allocation of executors.");
    options.add_options()
      ("p,port", "TCP port to listen on.",  cxxopts::value<int>()->default_value("8080"))
      ("processes", "Number of processeses to support.", cxxopts::value<int>()->default_value("1"))
      ("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"))
    ;
    auto parsed_options = options.parse(argc, argv);

    Options result;
    result.port = parsed_options["port"].as<int>();
    result.processes = parsed_options["processes"].as<int>();
    result.verbose = parsed_options["verbose"].as<bool>();

    return result;
  }


}
