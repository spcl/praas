
#include <praas/serving/docker/server.hpp>

#include <cxxopts.hpp>

namespace praas::serving::docker {

  Options opts(int argc, char** argv)
  {
    cxxopts::Options options(
        "praas-serving", "Handle client connections and allocation of processes."
    );
    options.add_options()
      ("p,port", "TCP port to listen on.",  cxxopts::value<int>()->default_value("8080"))
      ("max-processes", "Number of processes to support.", cxxopts::value<int>()->default_value("1"))
      ("threads", "Number of threads in the HTTP server.", cxxopts::value<int>()->default_value("1"))
      ("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"))
    ;
    auto parsed_options = options.parse(argc, argv);

    Options result;
    result.port = parsed_options["port"].as<int>();
    result.threads = parsed_options["threads"].as<int>();
    result.max_processes = parsed_options["max-processes"].as<int>();
    result.verbose = parsed_options["verbose"].as<bool>();

    return result;
  }

} // namespace praas::serving::docker
