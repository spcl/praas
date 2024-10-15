#include <praas/control-plane/backend.hpp>

#include <praas/common/exceptions.hpp>
#include <praas/common/http.hpp>
#include <praas/common/util.hpp>
#include <praas/control-plane/application.hpp>
#include <praas/control-plane/config.hpp>
#include <praas/control-plane/deployment.hpp>

#if defined(WITH_FARGATE_BACKEND)
#include <aws/core/client/AsyncCallerContext.h>
#include <aws/ec2/model/DescribeNetworkInterfacesRequest.h>
#include <aws/ec2/model/DescribeNetworkInterfacesResponse.h>
#include <aws/ec2/model/IpamPoolAwsService.h>
#include <aws/ecs/ECSServiceClientModel.h>
#include <aws/ecs/model/AssignPublicIp.h>
#include <aws/ecs/model/DescribeTasksRequest.h>
#include <aws/ecs/model/DescribeTasksResult.h>
#include <aws/ecs/model/LaunchType.h>
#include <aws/ecs/model/RunTaskRequest.h>
#include <aws/ecs/model/RunTaskResult.h>
#endif

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

  std::unique_ptr<Backend> Backend::construct(const config::Config& cfg, deployment::Deployment& deployment)
  {
    if (cfg.backend_type == Type::DOCKER) {
      return std::make_unique<DockerBackend>(
          *dynamic_cast<config::BackendDocker*>(cfg.backend.get()),
          deployment
      );
    }
#if defined(WITH_FARGATE_BACKEND)
    if (cfg.backend_type == Type::AWS_FARGATE) {
      return std::make_unique<FargateBackend>(
          *dynamic_cast<config::BackendFargate*>(cfg.backend.get()),
          deployment
      );
    }
#endif
    return nullptr;
  }

  DockerBackend::DockerBackend(const config::BackendDocker& cfg, deployment::Deployment& deployment):
    Backend(deployment)
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

    if(process->state().swap) {
      body["swap-location"] = process->state().swap->path(process->name());
    }

    if(auto * ptr = dynamic_cast<deployment::AWS*>(&_deployment)) {
      body["s3-swapping-bucket"] = ptr->s3_bucket;
    }

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

            spdlog::info("Received callback");
            callback(std::make_shared<DockerInstance>(ip_address, port, container), std::nullopt);

          } else {
            callback(nullptr, fmt::format("Unknown error! Response: {}", response->getBody()));
          }
        }
    );
  }

  void DockerBackend::shutdown(
    const std::string& name,
    process::ProcessObserver instance,
    std::function<void(std::optional<std::string>)>&& callback
  )
  {
    _http_client.post(
        "/kill",
        {
            {"process", name},
        },
        [callback = std::move(callback), name, instance, this](
          drogon::ReqResult result, const drogon::HttpResponsePtr& response
        ) mutable {

          if (result != drogon::ReqResult::Ok) {
            callback(fmt::format("Unknown error!"));
            return;
          }

          if (response->getStatusCode() == drogon::HttpStatusCode::k500InternalServerError) {
            callback(
              fmt::format(
                  "Process {} could not stopped, reason: {}", name, response->body()
              )
            );

          } else if (response->getStatusCode() == drogon::HttpStatusCode::k200OK) {

            auto ptr = instance.lock();
            if(ptr) {
              std::erase(_instances, ptr->handle_ptr());
              ptr->reset_handle();
              callback(std::nullopt);
            } else {
              callback("Fatal error: process deleted during stopping!");
            }


          } else {
            callback(fmt::format("Unknown error! Response: {}", response->getBody()));
          }
        }
    );
  }

  double DockerBackend::max_memory() const
  {
    return 1024;
  }

  double DockerBackend::max_vcpus() const
  {
    return 1;
  }

#if defined(WITH_FARGATE_BACKEND)
  FargateBackend::FargateBackend(const config::BackendFargate& cfg, deployment::Deployment& deployment):
    Backend(deployment)
  {
    _logger = common::util::create_logger("FargateBackend");

    // https://github.com/aws/aws-sdk-cpp/issues/1410
    putenv("AWS_EC2_METADATA_DISABLED=true");

    InitAPI(_options);
    _client = std::make_shared<Aws::ECS::ECSClient>();
    _ec2_client = std::make_shared<Aws::EC2::EC2Client>();

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

  void FargateBackend::FargateInstance::
      _callback_task_describe(const Aws::ECS::ECSClient* /*unused*/, const Aws::ECS::Model::DescribeTasksRequest&, const Aws::ECS::Model::DescribeTasksOutcome& result, const std::shared_ptr<const Aws::Client::AsyncCallerContext>&)
  {
    if (result.IsSuccess()) {

      if (result.GetResult().GetTasks().empty()) {
        this->_connected_callback(
            fmt::format("Couldn't query the ENI interface of task {}", container_id)
        );
        return;
      }

      if (result.GetResult().GetTasks()[0].GetAttachments().empty()) {
        this->_connected_callback(
            fmt::format("Couldn't query the ENI interface of task {}", container_id)
        );
        return;
      }

      for (auto& obj : result.GetResult().GetTasks()[0].GetAttachments()[0].GetDetails()) {
        if (obj.GetName() == "networkInterfaceId") {
          eni_interface = obj.GetValue();
        }
      }

    } else {
      this->_connected_callback(fmt::format(
          "Couldn't query the ENI interface of task {}, error {}", container_id,
          result.GetError().GetMessage()
      ));
      return;
    }

    if (eni_interface.empty()) {
      this->_connected_callback(
          fmt::format("Couldn't query the ENI interface of task {}", container_id)
      );
      return;
    }

    Aws::EC2::Model::DescribeNetworkInterfacesRequest req;
    req.SetNetworkInterfaceIds({eni_interface});

    _ec2_client->DescribeNetworkInterfacesAsync(
        [this](auto* ptr, auto res, auto outcome, auto& context) mutable {
          _callback_describe_eni(ptr, res, outcome, context);
        },
        nullptr,
        req
    );
  }

  void FargateBackend::FargateInstance::
      _callback_describe_eni(const Aws::EC2::EC2Client* /*unused*/, const Aws::EC2::Model::DescribeNetworkInterfacesRequest&, const Aws::EC2::Model::DescribeNetworkInterfacesOutcome& result, const std::shared_ptr<const Aws::Client::AsyncCallerContext>&)
  {
    if (result.IsSuccess()) {

      if (result.GetResult().GetNetworkInterfaces().empty()) {
        this->_connected_callback(
            fmt::format("Couldn't query the IP address of ENI interface {}", eni_interface)
        );
      } else {
        this->ip_address =
            result.GetResult().GetNetworkInterfaces()[0].GetAssociation().GetPublicIp();

        this->_connected_callback(std::nullopt);
      };
    } else {
      this->_connected_callback(
          fmt::format("Fargate initialization failed, reason: {}", result.GetError().GetMessage())
      );
    }
  }

  void FargateBackend::FargateInstance::connect(
      std::function<void(const std::optional<std::string>&)>&& callback
  )
  {
    this->_connected_callback = std::move(callback);

    // Find the ENI interface
    Aws::ECS::Model::DescribeTasksRequest req;
    req.WithCluster(cluster_name);
    // req.SetTasks({"7ed0afc1-1cef-4570-8740-5cab2d221bdf"});
    req.SetTasks({container_id});

    _ecs_client->DescribeTasksAsync(
        req,
        [this](auto* ptr, auto res, auto outcome, auto& context) mutable {
          _callback_task_describe(ptr, res, outcome, context);
        }
    );
  }

  void FargateBackend::allocate_process(
      process::ProcessPtr process, const process::Resources& resources,
      std::function<void(std::shared_ptr<ProcessInstance>&&, std::optional<std::string>)>&& callback
  )
  {
    // FIXME: swap bucket
    std::string cluster_name = _fargate_config["cluster_name"].asString();

    Aws::ECS::Model::RunTaskRequest req;
    req.SetCluster(_fargate_config["cluster_name"].asString());
    req.SetTaskDefinition(process->application().resources().code_resource_name);
    req.SetLaunchType(Aws::ECS::Model::LaunchType::FARGATE);

    Aws::ECS::Model::NetworkConfiguration net_cfg;
    Aws::ECS::Model::AwsVpcConfiguration aws_net_cfg;
    auto subnet = _fargate_config["subnet"].asString();
    aws_net_cfg.SetSubnets({subnet});
    aws_net_cfg.SetAssignPublicIp(Aws::ECS::Model::AssignPublicIp::ENABLED);
    net_cfg.SetAwsvpcConfiguration(aws_net_cfg);
    req.SetNetworkConfiguration(net_cfg);

    Aws::ECS::Model::TaskOverride task_override;
    task_override.SetCpu(resources.vcpus);
    task_override.SetMemory(resources.memory);

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
        [=, callback = std::move(callback),
         this](const Aws::ECS::ECSClient* /*unused*/, const Aws::ECS::Model::RunTaskRequest&, const Aws::ECS::Model::RunTaskOutcome& result, const std::shared_ptr<const Aws::Client::AsyncCallerContext>&) mutable {
          if (result.IsSuccess()) {
            const auto& task = result.GetResult().GetTasks().front();
            callback(
                std::make_shared<FargateInstance>(
                    8000, task.GetTaskArn(), cluster_name, _client, _ec2_client
                ),
                std::nullopt
            );
          } else {
            callback(nullptr, fmt::format("Fargate error: {}", result.GetError().GetMessage()));
          }
        }
    );
  }

  void FargateBackend::shutdown(
    const std::string& name,
    process::ProcessObserver instance,
    std::function<void(std::optional<std::string>)>&& callback
  )
  {
    // FIXME: send call to erase
    std::erase(_instances, instance.lock()->handle_ptr());
  }

  double FargateBackend::max_memory() const
  {
    return 120;
  }

  double FargateBackend::max_vcpus() const
  {
    return 16384;
  }
#endif

} // namespace praas::control_plane::backend
