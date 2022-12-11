
#include <praas/common/exceptions.hpp>
#include <praas/common/messages.hpp>
#include <praas/common/util.hpp>
#include <praas/control-plane/worker.hpp>

#include <praas/control-plane/backend.hpp>
#include <praas/control-plane/resources.hpp>
#include <praas/control-plane/server.hpp>

#include <charconv>
#include <thread>

#include <spdlog/spdlog.h>
#include <sockpp/tcp_connector.h>

namespace praas::control_plane::worker {

  void
  Workers::handle_message(process::ProcessObserver* process, praas::common::message::Message msg)
  {
    auto parsed = msg.parse();
    auto ptr = process->lock();

    if (!ptr) {
      spdlog::error("Could not acquire pointer access to a process {} - deleted?", fmt::ptr(process));
      return;
    }
  }

  void Workers::handle_invocation(
    HttpServer::request_t request,
    HttpServer::callback_t && callback,
    const std::string& app_id,
    std::string function_name
  )
  {
    Resources::RWAccessor acc;
    _resources.get_application(app_id, acc);
    if(acc.empty()) {
      auto resp = HttpServer::failed_response("App unknown", drogon::k404NotFound);
      callback(resp);
      return;
    }

    // FIXME: make resources configurable
    common::util::assert_true(_server != nullptr);

    {
      // Get a process or allocate one.
      // FIXME: allocation should be non-blocking (HTTP request?)
      // FIXME: configure resources
      auto [lock, proc_ptr] = acc.get()->get_controlplane_process(
        _backend, *_server, process::Resources{1, 2048, ""}
      );

      proc_ptr->add_invocation(std::move(request), std::move(callback), function_name);
    }
  }

  bool Workers::create_application(std::string app_name)
  {
    try {
      _resources.add_application(Application{app_name});
    } catch (common::PraaSException &) {
     return false;
    }
    return true;
  }

  void
  Workers::handle_invocation_result(const process::ProcessPtr& ptr, const praas::common::message::InvocationResultParsed&)
  {
  }

  void Workers::handle_swap(const process::ProcessPtr& ptr)
  {}

  void
  Workers::handle_data_metrics(const process::ProcessPtr& ptr, const praas::common::message::DataPlaneMetricsParsed&)
  {}

  void Workers::handle_closure(const process::ProcessPtr& ptr)
  {}

  void Workers::swap(const process::ProcessPtr& ptr, state::SwapLocation& swap_loc)
  {}

  void Workers::invoke(const process::ProcessPtr& ptr, const praas::common::message::InvocationRequestParsed&)
  {}

} // namespace praas::control_plane::worker
