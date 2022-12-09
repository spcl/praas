#ifndef PRAAS_CONTROLL_PLANE_WORKER_HPP
#define PRAAS_CONTROLL_PLANE_WORKER_HPP

#include <praas/common/messages.hpp>
#include <praas/control-plane/config.hpp>
#include <praas/control-plane/process.hpp>

#include <BS_thread_pool.hpp>

namespace praas::control_plane::worker {

  class Workers {
  public:
    Workers(const config::Workers& config) : _pool(config.threads) {}

    void
    add_task(process::ProcessObserver* process, praas::common::message::Message&& message)
    {
      _pool.push_task(Workers::handle_message, process, message);
    }

    void handle_invocation(
      const process::ProcessPtr& ptr,
      std::string_view function_name,
      std::string_view invocation_id,
      std::string payload_data
    );

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
  };

} // namespace praas::control_plane::worker

#endif
