#ifndef PRAAS_CONTROLL_PLANE_WORKER_HPP
#define PRAAS_CONTROLL_PLANE_WORKER_HPP

#include <praas/common/messages.hpp>
#include <praas/common/util.hpp>
#include <praas/control-plane/config.hpp>
#include <praas/control-plane/deployment.hpp>
#include <praas/control-plane/http.hpp>
#include <praas/control-plane/process.hpp>
#include <praas/control-plane/resources.hpp>

#include <BS_thread_pool.hpp>

namespace praas::control_plane {
  struct Resources;
} // namespace praas::control_plane

namespace praas::control_plane::tcpserver {
  struct TCPServer;
} // namespace praas::control_plane::tcpserver

namespace praas::control_plane::worker {

  class Workers {
  public:
    Workers(
        const config::Workers& config, backend::Backend& backend,
        deployment::Deployment& deployment, Resources& resources
    )
        : _pool(config.threads), _resources(resources), _backend(backend), _deployment(deployment)
    {
      _logger = common::util::create_logger("Workers");
    }

    void attach_tcpserver(tcpserver::TCPServer& server)
    {
      _server = &server;
    }

    void add_task(process::ProcessObserver* process, praas::common::message::Message&& message)
    {
      _pool.push_task(Workers::handle_message, process, message);
    }

    template <typename F, typename... Args>
    void add_task(F&& func, Args&&... args)
    {
      _pool.push_task([this, func, ... args = std::forward<Args>(args)]() mutable {
        std::invoke(func, *this, std::forward<Args>(args)...);
      });
    }

    template <typename F>
    void add_task(F&& func)
    {
      _pool.push_task(std::forward<F>(func));
    }

    void handle_invocation(
        HttpServer::request_t request, HttpServer::callback_t&& callback, const std::string& app_id,
        std::string function_name
    );

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief Creates a PraaS application with a given name. Succeeds only if an
    /// application with such name does not exist.
    ///
    /// @param [in] name new application name
    /// @return true if application has been created; false otherwise
    ////////////////////////////////////////////////////////////////////////////////
    bool create_application(const std::string& app_name, ApplicationResources&& cloud_resources);

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief Creates a process within an existing application.
    /// This launches process creation in the background that will be resolved later.
    /// In practice, this means sending an asynchronous HTTP request.
    ///
    /// @param [in] app_name application name
    /// @param [in] proc_id new process name
    /// @param [in] resources process resource specification
    /// @return true if a process has been created in an application
    ////////////////////////////////////////////////////////////////////////////////
    bool create_process(
        const std::string& app_name, const std::string& proc_id, process::Resources&& resources
    );

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief Delete a process and its swapped state. Applies only to processes
    /// that have been swapped out.
    ///
    /// @param [in] app_name application name
    /// @param [in] proc_id new process name
    /// @return error message if operation failed; empty optional otherwise
    ////////////////////////////////////////////////////////////////////////////////
    std::optional<std::string>
    delete_process(const std::string& app_name, const std::string& proc_id);

  private:
    // Looks up the associated invocation in a process and calls the callback.
    // Requires a read access to the list of invocations.
    static void
    handle_invocation_result(const process::ProcessPtr& ptr, const praas::common::message::InvocationResultParsed&);

    // Calls to process to finish and swap.
    // Needs to call the application to handle the change of process state.
    static void handle_swap(const process::ProcessPtr& ptr);

    // Update data plane metrics of a process
    // Requires write access to this process component.
    static void
    handle_data_metrics(const process::ProcessPtr& ptr, const praas::common::message::DataPlaneMetricsParsed&);

    // Close down a process.
    // Requires write access to the application.
    static void handle_closure(const process::ProcessPtr& ptr);

    // Starts the swap operation.
    // Requires a write operation to process to lock all future invocations.
    static void swap(const process::ProcessPtr& ptr, state::SwapLocation& swap_loc);

    // Adds an invocation request and sends the payload.
    // Requires write access to list of invocations.
    // Requires write access to the socket.
    static void
    invoke(const process::ProcessPtr& ptr, const praas::common::message::InvocationRequestParsed&);

    // Generic message processing
    static void handle_message(process::ProcessObserver* process, praas::common::message::Message);

    BS::thread_pool _pool;

    Resources& _resources;

    backend::Backend& _backend;

    deployment::Deployment& _deployment;

    tcpserver::TCPServer* _server{};

    std::shared_ptr<spdlog::logger> _logger;
  };

} // namespace praas::control_plane::worker

#endif
