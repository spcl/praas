#include <praas/control-plane/worker.hpp>

#include <praas/common/exceptions.hpp>
#include <praas/common/messages.hpp>
#include <praas/common/util.hpp>
#include <praas/control-plane/application.hpp>
#include <praas/control-plane/backend.hpp>
#include <praas/control-plane/process.hpp>
#include <praas/control-plane/resources.hpp>
#include <praas/control-plane/server.hpp>

#include <charconv>
#include <chrono>
#include <thread>

#include <drogon/HttpTypes.h>
#include <sockpp/tcp_connector.h>
#include <spdlog/spdlog.h>

namespace praas::control_plane::worker {

  void Workers::handle_invocation(
      HttpServer::request_t request, HttpServer::callback_t&& callback, const std::string& app_id,
      std::string function_name, std::chrono::high_resolution_clock::time_point start
  )
  {
    Resources::RWAccessor acc;
    _resources.get_application(app_id, acc);
    if (acc.empty()) {
      auto resp = HttpServer::failed_response("App unknown", drogon::k404NotFound);
      callback(resp);
      return;
    }

    common::util::assert_true(_server != nullptr);

    {
      // Get a process or allocate one.
      // FIXME: make resources configurable
      acc.get()->get_controlplane_process(
          _backend, *_server, process::Resources{"1", "2048", ""},
          [start, function_name, request = std::move(request), callback = std::move(callback)](
              process::ProcessPtr proc_ptr, const std::optional<std::string>& error_msg
          ) mutable {
            if (proc_ptr) {
              proc_ptr->write_lock();
              proc_ptr->add_invocation(
                  std::move(request), std::move(callback), function_name, start
              );
            } else {
              callback(
                  HttpServer::failed_response(error_msg.value(), drogon::k500InternalServerError)
              );
            }
          }
      );
    }
  }

  bool
  Workers::create_application(const std::string& app_name, ApplicationResources&& cloud_resources)
  {
    try {
      _resources.add_application(Application{app_name, std::move(cloud_resources)});
    } catch (common::PraaSException&) {
      return false;
    }
    return true;
  }

  bool
  Workers::delete_application(const std::string& app_name)
  {
    try {
      _resources.delete_application(app_name);
      return true;
    } catch (common::PraaSException&) {
      return false;
    }
  }

  bool Workers::create_process(
      const std::string& app_name, const std::string& proc_id, // NOLINT
      process::Resources&& resources,
      std::function<void(process::ProcessPtr, const std::optional<std::string>&)>&& callback
  )
  {
    Resources::RWAccessor acc;
    _resources.get_application(app_name, acc);
    if (acc.empty()) {
      return false;
    }

    acc.get()->add_process(
        this->_backend, *this->_server, proc_id, std::move(resources), std::move(callback)
    );
    return true;
  }

  void Workers::stop_process(
    const std::string& app_name, const std::string& proc_id,
    std::function<void(const std::optional<std::string>&)>&& callback
  )
  {
    Resources::RWAccessor acc;
    _resources.get_application(app_name, acc);
    if (acc.empty()) {
      callback("Application does not exist");
    }

    try {
      acc.get()->stop_process(proc_id, this->_backend, std::move(callback));
    } catch (common::ObjectDoesNotExist&) {
      callback("Process does not exist or is not alive.");
    }
  }

  std::optional<std::string>
  Workers::delete_process(const std::string& app_name, const std::string& proc_id)
  {
    Resources::RWAccessor acc;
    _resources.get_application(app_name, acc);
    if (acc.empty()) {
      return "Application does not exist";
    }

    try {
      acc.get()->delete_process(proc_id, this->_deployment);
      return std::nullopt;
    } catch (common::ObjectDoesNotExist&) {
      return "Process does not exist or is not swapped out.";
    }
  }

  std::optional<std::string>
  Workers::swap_process(const std::string& app_name, const std::string& proc_id)
  {

    Resources::RWAccessor acc;
    _resources.get_application(app_name, acc);
    if (acc.empty()) {
      return "Application does not exist";
    }

    try {
      acc.get()->swap_process(proc_id, this->_deployment);
      return std::nullopt;
    } catch (common::ObjectDoesNotExist&) {
      return "Process does not exist.";
    } catch (common::InvalidProcessState&) {
      return "Process cannot be swapped out (not allocated, not active).";
    }
  }

  std::optional<std::string> Workers::list_processes(
      const std::string& app_name, std::vector<std::string>& active_processes,
      std::vector<std::string>& swapped_processes
  )
  {
    Resources::ROAccessor acc;
    _resources.get_application(app_name, acc);
    if (acc.empty()) {
      return "Application does not exist";
    }
    acc.get()->get_processes(active_processes);
    acc.get()->get_swapped_processes(swapped_processes);
    return std::nullopt;
  }

  void Workers::
      handle_invocation_result(const process::ProcessPtr& ptr, const praas::common::message::InvocationResultPtr&)
  {
  }

  void Workers::handle_swap(const process::ProcessPtr& ptr) {}

  void Workers::
      handle_data_metrics(const process::ProcessPtr& ptr, const praas::common::message::DataPlaneMetricsPtr&)
  {
  }

  void Workers::handle_closure(const process::ProcessPtr& ptr) {}

  void Workers::
      invoke(const process::ProcessPtr& ptr, const praas::common::message::InvocationRequestPtr&)
  {
  }

} // namespace praas::control_plane::worker
