
#include <praas/common/http.hpp>
#include <praas/control-plane/config.hpp>
#include <praas/control-plane/http.hpp>

#include <praas/common/util.hpp>
#include <praas/control-plane/process.hpp>
#include <praas/control-plane/resources.hpp>
#include <praas/control-plane/worker.hpp>

#include <chrono>
#include <drogon/HttpAppFramework.h>
#include <drogon/HttpTypes.h>
#include <future>
#include <thread>

#include <spdlog/spdlog.h>

namespace praas::control_plane {

  HttpServer::HttpServer(config::HTTPServer& cfg, worker::Workers& workers)
      : _port(cfg.port), _threads(cfg.threads), _workers(workers)
  {
    _logger = common::util::create_logger("HttpServer");
    drogon::app().setClientMaxBodySize(cfg.max_payload_size);
  }

  void HttpServer::run()
  {
    drogon::app().registerController(shared_from_this());
    drogon::app().setThreadNum(_threads);
    _server_thread = std::thread{[this]() { drogon::app().addListener("0.0.0.0", _port).run(); }};
  }

  void HttpServer::shutdown()
  {
    _logger->info("Stopping HTTP server");
    if (drogon::app().isRunning()) {
      drogon::app().getLoop()->queueInLoop([]() { drogon::app().quit(); });
    }
    if (_server_thread.joinable()) {
      _server_thread.join();
    }
    _logger->info("Stopped HTTP server");
  }

  void HttpServer::wait()
  {
    _server_thread.join();
  }

  drogon::HttpResponsePtr HttpServer::correct_response(const std::string& reason)
  {
    Json::Value json;
    json["message"] = reason;
    auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
    resp->setStatusCode(drogon::k200OK);
    return resp;
  }

  drogon::HttpResponsePtr
  HttpServer::failed_response(const std::string& reason, drogon::HttpStatusCode status_code)
  {
    Json::Value json;
    json["reason"] = reason;
    auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
    resp->setStatusCode(status_code);
    return resp;
  }

  void HttpServer::create_app(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback, const std::string& app_name
  )
  {
    std::string cloud_resource_name = request->getParameter("cloud_resource_name");
    if (cloud_resource_name.empty()) {
      callback(failed_response("Missing arguments!"));
      return;
    }

    _logger->info("Create new application {}", app_name);
    _workers.add_task([=, callback = std::move(callback), this]() {
      if (_workers.create_application(app_name, ApplicationResources{cloud_resource_name})) {
        callback(correct_response("Created"));
      } else {
        callback(failed_response("Failed to create"));
      }
    });
  }

  void HttpServer::create_process(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
      const std::string& app_name, // NOLINT
      const std::string& process_name
  )
  {
    std::string vcpus_str = request->getParameter("vcpus");
    std::string memory_str = request->getParameter("memory");
    if (vcpus_str.empty() || memory_str.empty()) {
      callback(failed_response("Missing arguments!"));
    }

    _workers.add_task(
        &worker::Workers::create_process, app_name, process_name,
        process::Resources{vcpus_str, memory_str, ""},
        [callback =
             std::move(callback)](process::ProcessPtr proc, std::optional<std::string> error_msg) {
          if (proc) {

            Json::Value conn_description;
            conn_description["type"] = "direct";
            conn_description["ip-address"] = proc->handle().ip_address;
            conn_description["port"] = proc->handle().port;

            Json::Value proc_description;
            proc_description["name"] = proc->name();
            proc_description["connection"] = conn_description;

            callback(common::http::HTTPClient::correct_response(proc_description));
          } else {
            callback(failed_response(error_msg.value()));
          }
        }
    );
  }

  void HttpServer::invoke(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback, const std::string& app_name,
      const std::string& function_name
  )
  {
    _logger->info("Push new invocation request of {}", function_name);
    auto start = std::chrono::high_resolution_clock::now();
    _workers.add_task(
        &worker::Workers::handle_invocation, request, std::move(callback), app_name, function_name,
        start
    );
  }

  void HttpServer::delete_process(
      const drogon::HttpRequestPtr&, // NOLINT
      std::function<void(const drogon::HttpResponsePtr&)>&& callback, const std::string& app_name,
      const std::string& process_name
  )
  {
    _logger->info("Delete process {}", process_name);
    _workers.add_task([=, this, callback = std::move(callback)]() {
      auto response = _workers.delete_process(app_name, process_name);
      if (response) {
        callback(failed_response(response.value(), drogon::HttpStatusCode::k400BadRequest));
      } else {
        callback(correct_response(fmt::format("Deleted swapped state of process {}.", process_name))
        );
      }
    });
  }

  void HttpServer::swap_process(
      const drogon::HttpRequestPtr&, // NOLINT,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback, const std::string& app_name,
      const std::string& process_name
  )
  {
    _logger->info("Swap process {}", process_name);
    _workers.add_task([=, this, callback = std::move(callback)]() {
      auto response = _workers.swap_process(app_name, process_name);
      if (response) {
        callback(failed_response(response.value(), drogon::HttpStatusCode::k400BadRequest));
      } else {
        callback(correct_response(fmt::format("Request swapping of process {}.", process_name)));
      }
    });
  }

  void HttpServer::list_processes(
      const drogon::HttpRequestPtr&, std::function<void(const drogon::HttpResponsePtr&)>&& callback,
      const std::string& app_name
  )
  {
    _workers.add_task([=, this, callback = std::move(callback)]() {
      std::vector<std::string> active_processes;
      std::vector<std::string> swapped_processes;

      auto response = _workers.list_processes(app_name, active_processes, swapped_processes);
      if (response) {
        callback(failed_response(response.value(), drogon::HttpStatusCode::k400BadRequest));
      }

      Json::Value json;
      json["application"] = app_name;

      if (!active_processes.empty()) {
        Json::Value active;
        for (const auto& proc_name : active_processes) {
          active.append(proc_name);
        }
        json["active"] = active;
      }

      if (!swapped_processes.empty()) {
        Json::Value swapped;
        for (const auto& proc_name : swapped_processes) {
          swapped.append(proc_name);
        }
        json["swapped"] = swapped;
      }

      auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
      resp->setStatusCode(drogon::HttpStatusCode::k200OK);
      callback(resp);
    });
  }

} // namespace praas::control_plane
