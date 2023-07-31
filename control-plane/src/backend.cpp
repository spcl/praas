#include <praas/control-plane/backend.hpp>

#include <praas/common/exceptions.hpp>
#include <praas/common/http.hpp>
#include <praas/common/util.hpp>
#include <praas/control-plane/application.hpp>
#include <praas/control-plane/config.hpp>

#include <fcntl.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <unistd.h>

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
    return nullptr;
  }

  DockerBackend::DockerBackend(const config::BackendDocker& cfg)
  {
    _logger = common::util::create_logger("LocalBackend");

    _http_client = common::http::HTTPClientFactory::create_client(cfg.address, cfg.port);
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
        [callback = std::move(callback), this, process = std::move(process)](
            drogon::ReqResult result, const drogon::HttpResponsePtr& response
        ) mutable {
          if (response->getStatusCode() == drogon::HttpStatusCode::k500InternalServerError) {

            callback(
                nullptr,
                fmt::format(
                    "Process {} could not be created, reason: {}", process->name(), response->body()
                )
            );

          } else if (response->getStatusCode() == drogon::HttpStatusCode::k201Created) {

            auto container = (*response->jsonObject())["container-id"].asString();
            callback(std::make_shared<DockerInstance>(container), std::nullopt);

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

} // namespace praas::control_plane::backend
