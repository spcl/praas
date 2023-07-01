#ifndef PRAAS_CONTROLL_PLANE_WORKER_HPP
#define PRAAS_CONTROLL_PLANE_WORKER_HPP

#include <praas/common/messages.hpp>
#include <praas/common/util.hpp>
#include <praas/control-plane/config.hpp>
#include <praas/control-plane/http.hpp>
#include <praas/control-plane/process.hpp>

#include <BS_thread_pool.hpp>

namespace praas::control_plane {
  struct Resources;
}

namespace praas::control_plane::tcpserver {
  struct TCPServer;
}

namespace praas::control_plane::worker {

  class Workers {
  public:
    Workers(const config::Workers& config, backend::Backend& backend, Resources & resources) :
      _pool(config.threads),
      _resources(resources),
      _backend(backend)
    {
      _logger = common::util::create_logger("Workers");
    }

    void attach_tcpserver(tcpserver::TCPServer & server)
    {
      _server = &server;
    }

    void
    add_task(process::ProcessObserver* process, praas::common::message::Message&& message)
    {
      _pool.push_task(Workers::handle_message, process, message);
    }

    template<typename F, typename... Args>
    void add_task(F && f, Args &&... args)
    {
      _pool.push_task(
        [this, f, ...args = std::forward<Args>(args)]() mutable {
          std::invoke(f, *this, std::forward<Args>(args)...);
        }
      );
    }

    template<typename F>
    void add_task(F && f)
    {
      _pool.push_task(std::forward<F>(f));
    }

    void handle_invocation(
      HttpServer::request_t request,
      HttpServer::callback_t && callback,
      const std::string& app_id,
      std::string function_name
    );

    bool create_application(std::string name);

    bool create_process(std::string name);

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
    static void invoke(const process::ProcessPtr& ptr, const praas::common::message::InvocationRequestParsed&);

    // Generic message processing
    static void
    handle_message(process::ProcessObserver* process, praas::common::message::Message);

    BS::thread_pool _pool;

    Resources& _resources;

    backend::Backend& _backend;

    tcpserver::TCPServer* _server;

    std::shared_ptr<spdlog::logger> _logger;
  };

} // namespace praas::control_plane::worker

#endif
