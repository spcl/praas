#include <praas/sdk/process.hpp>

#include <spdlog/spdlog.h>

int main(int argc, char** argv)
{
  //auto config = praas::process::config::Controller::deserialize(argc, argv);
  //if (config.verbose)
  //  spdlog::set_level(spdlog::level::debug);
  //else
  //  spdlog::set_level(spdlog::level::info);
  spdlog::set_pattern("[%H:%M:%S:%f] [P %P] [T %t] [%l] %v ");
  spdlog::info("Executing PraaS controller!");

  std::string addr = "127.0.0.1";
  int port = 8000;

  praas::sdk::Process process{addr, port};
  if(!process.connect())  {
    std::cerr << "Could not connect" << std::endl;
    return 1;
  }

  int BUF_LEN = 1024;
  std::unique_ptr<char[]> buf{new char[BUF_LEN]};
  for(int i = 0; i < BUF_LEN / 4; ++i)
    ((int*)buf.get())[i] = i + 1;

  auto result = process.invoke("no_op", "id", buf.get(), BUF_LEN);

  for(int i = (BUF_LEN/4) - 5; i < BUF_LEN / 4; ++i) {
    std::cerr << i << " " << ((int*)buf.get())[i] << std::endl;
    std::cerr << i << " " << ((int*)result.payload.get())[i] << std::endl;
  }
  process.disconnect();

  spdlog::info("Process controller is closing down");
  return 0;
}
