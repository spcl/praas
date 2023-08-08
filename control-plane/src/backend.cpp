#include <praas/control-plane/backend.hpp>

#include <praas/common/exceptions.hpp>
#include <praas/common/http.hpp>
#include <praas/common/util.hpp>
#include <praas/control-plane/application.hpp>
#include <praas/control-plane/config.hpp>

#include <aws/ecs/ECSClient.h>
#include <aws/ecs/ECSServiceClientModel.h>
#include <aws/ecs/model/AssignPublicIp.h>
#include <aws/ecs/model/LaunchType.h>
#include <aws/ecs/model/RunTaskRequest.h>
#include <aws/ecs/model/RunTaskResult.h>
#include <drogon/HttpTypes.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <fstream>

namespace praas::control_plane::backend {

  Type deserialize(std::string mode)
  {
    if (mode == "docker") {
      return Type::DOCKER;
    } else if (mode == "aws_fargate") {
      return Type::AWS_FARGATE;
    } else if (mode == "aws_lambda") {
      return Type::AWS_LAMBDA;
    } else {
      return Type::NONE;
    }
  }

  std::unique_ptr<Backend> Backend::construct(const config::Config& cfg)
  {
    if (cfg.backend_type == Type::DOCKER) {
      return std::make_unique<DockerBackend>(*dynamic_cast<config::BackendDocker*>(cfg.backend.get()
      ));
    }
    if (cfg.backend_type == Type::AWS_FARGATE) {
      return std::make_unique<FargateBackend>(
          *dynamic_cast<config::BackendFargate*>(cfg.backend.get())
      );
    }
    return nullptr;
  }

  DockerBackend::DockerBackend(const config::BackendDocker& cfg)
  {
    _logger = common::util::create_logger("LocalBackend");

    _http_client = common::http::HTTPClientFactory::create_client(
        fmt::format("http://{}", cfg.address), cfg.port
    );
  }

  DockerBackend::~DockerBackend()
  {
    // FIXME: kill containers
  }

  void Backend::configure_tcpserver(const std::string& ip, int port)
  {
    _tcp_ip = ip;
    _tcp_port = port;
  }

  void DockerBackend::allocate_process(
      process::ProcessPtr process, const process::Resources& resources,
      std::function<void(std::shared_ptr<ProcessInstance>&&, std::optional<std::string>)>&& callback
  )
  {
    Json::Value body;
    body["container-name"] = process->application().resources().code_resource_name;
    body["controlplane-address"] = fmt::format("{}:{}", _tcp_ip, _tcp_port);

    _http_client.post(
        "/create",
        {
            {"process", process->name()},
        },
        std::move(body),
        [callback = std::move(callback), process,
         this](drogon::ReqResult result, const drogon::HttpResponsePtr& response) mutable {
          if (result != drogon::ReqResult::Ok) {
            callback(nullptr, fmt::format("Unknown error!"));
            return;
          }
          if (response->getStatusCode() == drogon::HttpStatusCode::k500InternalServerError) {
            callback(
                nullptr,
                fmt::format(
                    "Process {} could not be created, reason: {}", process->name(), response->body()
                )
            );

          } else if (response->getStatusCode() == drogon::HttpStatusCode::k200OK) {

            auto container = (*response->jsonObject())["container-id"].asString();
            auto ip_address = (*response->jsonObject())["ip-address"].asString();
            auto port = (*response->jsonObject())["port"].asInt();

            callback(std::make_shared<DockerInstance>(ip_address, port, container), std::nullopt);

          } else {
            callback(nullptr, fmt::format("Unknown error! Response: {}", response->getBody()));
          }
        }
    );
  }

  void DockerBackend::shutdown(const std::shared_ptr<ProcessInstance>& instance)
  {
    // FIXME: send call to erase
    std::erase(_instances, instance);
  }

  int DockerBackend::max_memory() const
  {
    return 1024;
  }

  int DockerBackend::max_vcpus() const
  {
    return 1;
  }

  FargateBackend::FargateBackend(const config::BackendFargate& cfg)
  {
    _logger = common::util::create_logger("FargateBackend");

    InitAPI(_options);
    _client = std::make_unique<Aws::ECS::ECSClient>();

    std::ifstream cfg_input{cfg.fargate_config};
    // FIXME: error handling
    if (cfg_input.is_open()) {
      cfg_input >> _fargate_config;
    }
  }

  FargateBackend::~FargateBackend()
  {
    // FIXME: kill tasks
    ShutdownAPI(_options);
  }

  void FargateBackend::allocate_process(
      process::ProcessPtr process, const process::Resources& resources,
      std::function<void(std::shared_ptr<ProcessInstance>&&, std::optional<std::string>)>&& callback
  )
  {
    Aws::ECS::Model::RunTaskRequest req;
    req.SetCluster(_fargate_config["cluster_name"].asString());
    req.SetTaskDefinition(_fargate_config["tasks"][0]["arn"].asString());
    req.SetLaunchType(Aws::ECS::Model::LaunchType::FARGATE);

    Aws::ECS::Model::NetworkConfiguration net_cfg;
    Aws::ECS::Model::AwsVpcConfiguration aws_net_cfg;
    auto subnet = _fargate_config["subnet"].asString();
    aws_net_cfg.SetSubnets({subnet});
    aws_net_cfg.SetAssignPublicIp(Aws::ECS::Model::AssignPublicIp::ENABLED);
    net_cfg.SetAwsvpcConfiguration(aws_net_cfg);
    req.SetNetworkConfiguration(net_cfg);

    Aws::ECS::Model::TaskOverride task_override;
    // FIXME: test and reenable
    // task_override.SetCpu(std::to_string(resources.vcpus));
    // task_override.SetMemory(std::to_string(resources.memory));

    Aws::ECS::Model::ContainerOverride env;
    auto controlplane_addr = fmt::format("{}:{}", _tcp_ip, _tcp_port);
    env.SetEnvironment(
        {Aws::ECS::Model::KeyValuePair{}.WithName("CONTROLPLANE_ADDR").WithValue(controlplane_addr),
         Aws::ECS::Model::KeyValuePair{}.WithName("PROCESS_ID").WithValue(process->name())}
    );
    env.SetName("process");

    task_override.SetContainerOverrides({env});
    req.SetOverrides(task_override);

    _client->RunTaskAsync(
        req,
        [callback = std::move(callback),
         this](const Aws::ECS::ECSClient* /*unused*/, const Aws::ECS::Model::RunTaskRequest&, const Aws::ECS::Model::RunTaskOutcome& result, const std::shared_ptr<const Aws::Client::AsyncCallerContext>&) mutable {
          if (result.IsSuccess()) {
            const auto& task = result.GetResult().GetTasks().front();
            callback(std::make_shared<FargateInstance>(8000, task.GetTaskArn()), std::nullopt);
          } else {
            _logger->error("Starting task failed!");
            _logger->error(result.GetResult().GetTasks().size());
            _logger->error(result.GetResult().GetFailures().size());
            _logger->error(result.GetError().GetMessage());
            for (auto& err : result.GetResult().GetFailures()) {
              _logger->error(err.GetReason());
            }
            callback(nullptr, "Fargate error");
          }
        }
    );
  }

  void FargateBackend::shutdown(const std::shared_ptr<ProcessInstance>& instance)
  {
    // FIXME: send call to erase
    std::erase(_instances, instance);
  }

  int FargateBackend::max_memory() const
  {
    return 1024;
  }

  int FargateBackend::max_vcpus() const
  {
    return 1;
  }

} // namespace praas::control_plane::backend
