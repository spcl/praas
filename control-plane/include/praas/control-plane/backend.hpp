#ifndef PRAAS_CONTROLL_PLANE_BACKEND_HPP
#define PRAAS_CONTROLL_PLANE_BACKEND_HPP

#include <praas/common/http.hpp>
#include <praas/control-plane/process.hpp>

#include <aws/core/Aws.h>
#include <aws/ec2/EC2Client.h>
#include <aws/ecs/ECSClient.h>

#include <memory>

namespace praas::control_plane::config {

  struct BackendDocker;
  struct BackendFargate;
  struct Config;

} // namespace praas::control_plane::config

namespace praas::control_plane::process {

  struct Resources;

} // namespace praas::control_plane::process

namespace Aws::Client {

  class AsyncCallerContext;

} // namespace Aws::Client

namespace Aws::ECS {

  class ECSClient;

} // namespace Aws::ECS

namespace Aws::EC2 {

  class EC2Client;

} // namespace Aws::EC2

namespace Aws::ECS::Model {

  class RunTaskRequest;
  class RunTaskResult;
  class DescribeTasksRequest;

} // namespace Aws::ECS::Model

namespace Aws::EC2::Model {

  class DescribeNetworkInterfacesRequest;

} // namespace Aws::EC2::Model

namespace praas::control_plane::backend {

  enum class Type { NONE = 0, DOCKER, AWS_FARGATE, AWS_LAMBDA };

  Type deserialize(std::string mode);

  struct ProcessInstance {

    ProcessInstance(std::string ip_address, int port)
        : ip_address(std::move(ip_address)), port(port)
    {
    }

    virtual void connect(std::function<void(const std::optional<std::string>&)>&& callback)
    {
      callback(std::nullopt);
    }

    virtual std::string id() const = 0;

    std::string ip_address;
    int port;
  };

  struct Backend {

    Backend() = default;
    Backend(const Backend&) = default;
    Backend(Backend&&) = delete;
    Backend& operator=(const Backend&) = default;
    Backend& operator=(Backend&&) = delete;
    virtual ~Backend() = default;

    /**
     * @brief Launches a new cloud process using the API specified by the cloud provider.
     * The method will overwrite
     *
     * @param {name} shared pointer to the process instance
     * @param resources [TODO:description]
     */
    virtual void allocate_process(
        process::ProcessPtr, const process::Resources& resources,
        std::function<void(std::shared_ptr<ProcessInstance>&&, std::optional<std::string>)>&&
            callback
    ) = 0;

    virtual void shutdown(const std::shared_ptr<ProcessInstance>&) = 0;

    /**
     * @brief The upper cap on a memory that can be allocated for a process.
     *
     * @return maximal memory in megabytes
     */
    virtual int max_memory() const = 0;

    /**
     * @brief The maximal number of vCPUs that canbe allocated for a single process.
     *
     * @return count of virtual CPUs
     */
    virtual int max_vcpus() const = 0;

    /**
     * @brief factory method that returns a new backend instance according to configuration choice.
     * The user specifies backend type in the configuration, and the method returns an initialized
     * instance.
     *
     * @param {name} initialized backend instance, where instance type is one of Backend's
     * childrens.
     */
    static std::unique_ptr<Backend> construct(const config::Config&);

    void configure_tcpserver(const std::string& ip, int port);

  protected:
    std::string _tcp_ip;
    int _tcp_port;
  };

  struct DockerBackend : Backend {

    // FIXME: static polymorphism?
    struct DockerInstance : public ProcessInstance {

      DockerInstance(std::string ip_address, int port, std::string container_id)
          : ProcessInstance(std::move(ip_address), port), container_id(container_id)
      {
      }

      std::string id() const override
      {
        return container_id;
      }

      std::string container_id;
    };

    DockerBackend(const config::BackendDocker& cfg);

    ~DockerBackend() override;

    void allocate_process(
        process::ProcessPtr, const process::Resources& resources,
        std::function<void(std::shared_ptr<ProcessInstance>&&, std::optional<std::string>)>&&
            callback
    ) override;

    void shutdown(const std::shared_ptr<ProcessInstance>&) override;

    int max_memory() const override;

    int max_vcpus() const override;

  private:
    std::shared_ptr<spdlog::logger> _logger;

    common::http::HTTPClient _http_client;

    std::vector<std::shared_ptr<ProcessInstance>> _instances;
  };

#if defined(WITH_FARGATE_BACKEND)

  struct FargateBackend : Backend {

    struct FargateInstance : public ProcessInstance {

      FargateInstance(
          int port, std::string container_id, std::string cluster_name,
          std::shared_ptr<Aws::ECS::ECSClient>& ecs_client,
          std::shared_ptr<Aws::EC2::EC2Client>& ec2_client
      )
          : ProcessInstance("", port), container_id(container_id), cluster_name(cluster_name),
            _connected_callback(nullptr), _ecs_client(ecs_client), _ec2_client(ec2_client)
      {
      }

      std::string id() const override
      {
        return container_id;
      }

      virtual void connect(std::function<void(const std::optional<std::string>&)>&& callback);

      std::string container_id;

      std::string cluster_name;

      std::string eni_interface{};

      std::function<void(const std::optional<std::string>&)> _connected_callback;

      void
      _callback_task_describe(const Aws::ECS::ECSClient* /*unused*/, const Aws::ECS::Model::DescribeTasksRequest&, const Aws::ECS::Model::DescribeTasksOutcome& result, const std::shared_ptr<const Aws::Client::AsyncCallerContext>&);
      void
      _callback_describe_eni(const Aws::EC2::EC2Client* /*unused*/, const Aws::EC2::Model::DescribeNetworkInterfacesRequest&, const Aws::EC2::Model::DescribeNetworkInterfacesOutcome& result, const std::shared_ptr<const Aws::Client::AsyncCallerContext>&);

      std::shared_ptr<Aws::ECS::ECSClient> _ecs_client;
      std::shared_ptr<Aws::EC2::EC2Client> _ec2_client;
    };

    FargateBackend(const config::BackendFargate& cfg);

    ~FargateBackend() override;

    void allocate_process(
        process::ProcessPtr, const process::Resources& resources,
        std::function<void(std::shared_ptr<ProcessInstance>&&, std::optional<std::string>)>&&
            callback
    ) override;

    void shutdown(const std::shared_ptr<ProcessInstance>&) override;

    int max_memory() const override;

    int max_vcpus() const override;

  private:
    std::shared_ptr<spdlog::logger> _logger;

    Json::Value _fargate_config;

    Aws::SDKOptions _options;
    std::shared_ptr<Aws::ECS::ECSClient> _client;
    std::shared_ptr<Aws::EC2::EC2Client> _ec2_client;

    std::vector<std::shared_ptr<ProcessInstance>> _instances;
  };
#endif

} // namespace praas::control_plane::backend

#endif
