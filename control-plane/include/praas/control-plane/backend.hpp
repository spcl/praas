#ifndef PRAAS_CONTROLL_PLANE_BACKEND_HPP
#define PRAAS_CONTROLL_PLANE_BACKEND_HPP

#include <praas/common/http.hpp>
#include <praas/control-plane/process.hpp>

#include <memory>

namespace praas::control_plane::config {

  struct BackendDocker;
  struct Config;

} // namespace praas::control_plane::config

namespace praas::control_plane::process {

  struct Resources;

} // namespace praas::control_plane::process

namespace praas::control_plane::backend {

  enum class Type { NONE = 0, DOCKER, AWS_FARGATE, AWS_LAMBDA };

  Type deserialize(std::string mode);

  struct ProcessInstance {
    virtual std::string id() const = 0;
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

      DockerInstance(std::string container_id) : container_id(container_id) {}

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

} // namespace praas::control_plane::backend

#endif
