#include <praas/common/util.hpp>
#include <praas/control-plane/backend.hpp>

#include <praas/control-plane/config.hpp>
#include <praas/common/exceptions.hpp>

#include <sys/signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>

namespace praas::control_plane::backend {

  Type deserialize(std::string mode)
  {
    if (mode == "local") {
      return Type::LOCAL;
    } else if (mode == "aws_fargate") {
      return Type::AWS_FARGATE;
    } else {
      return Type::NONE;
    }
  }

  std::unique_ptr<Backend> Backend::construct(const config::Config& cfg)
  {
    if(cfg.backend_type == Type::LOCAL) {
      return std::make_unique<LocalBackend>();
    }
    return nullptr;
  }

  LocalBackend::LocalBackend()
  {
    _logger = common::util::create_logger("LocalBackend");
  }

  LocalBackend::~LocalBackend()
  {
    for(auto & instance : _instances) {
      shutdown(instance);
    }
  }

  void Backend::configure_tcpserver(const std::string& ip, int port)
  {
    _tcp_ip = ip;
    _tcp_port = port;
  }

  std::shared_ptr<ProcessInstance> LocalBackend::allocate_process(
    process::ProcessPtr ptr, const process::Resources& resources
  )
  {
    int mypid = fork();
    if (mypid < 0) {
      throw praas::common::PraaSException{fmt::format(
        "Fork failed! {}, reason {} {}", mypid, errno, strerror(errno))
      };
    }

    std::string proc_name = ptr->name();
    std::string control_plane_addr = fmt::format("{}:{}", _tcp_ip, _tcp_port);
    // FIXME: full path
    std::string code_location = "/work/serverless/2022/praas/code/build_debug/process/tests/integration/";
    std::string config_location = "configuration.json";

    std::cerr << mypid << std::endl;

    if (mypid == 0) {

      mypid = getpid();
      auto out_file = ("process_" + std::to_string(mypid));

      spdlog::info("Process begins work on PID {}", mypid);

      int fd = open(out_file.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
      dup2(fd, 1);
      dup2(fd, 2);

      setenv("PROCESS_ID", proc_name.c_str(), 1);
      setenv("CODE_LOCATION", code_location.c_str(), 1);
      setenv("CONFIG_LOCATION", config_location.c_str(), 1);
      setenv("CONTROLPLANE_ADDR", control_plane_addr.c_str(), 1);

      // FIXME: configure path!
      const char* args[] = {
        "/work/serverless/2022/praas/code/build_debug/process/bin/process_exe",
        nullptr
      };

      int ret = execvp(args[0], const_cast<char**>(&args[0]));
      if (ret == -1) {
        spdlog::error("Invoking process {} failed {}, reason {}", args[0], errno, strerror(errno));
        close(fd);
        exit(1);
      }

    } else {
      spdlog::info("Started process with PID {}", mypid);
      _instances.emplace_back(std::make_shared<LocalInstance>(mypid));
      return _instances.back();
    }

    return nullptr;
  }

  void LocalBackend::shutdown(const std::shared_ptr<ProcessInstance> & instance)
  {
    auto* ptr = dynamic_cast<LocalInstance*>(instance.get());
    kill(ptr->pid, SIGINT);

    int status{};
    waitpid(ptr->pid, &status, 0);

    if (WIFEXITED(status)) {
      _logger->info("Process instance {} exited with status {}", ptr->pid, WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
      _logger->info("Process instance {} killed by signal {}", ptr->pid, WTERMSIG(status));
    }

    std::erase(_instances, instance);
  }

  int LocalBackend::max_memory() const
  {
    return 1024;
  }

  int LocalBackend::max_vcpus() const
  {
    return 1;
  }

} // namespace praas::control_plane::backend
