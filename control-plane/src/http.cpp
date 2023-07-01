
#include <praas/control-plane/config.hpp>
#include <praas/control-plane/http.hpp>

#include <praas/common/util.hpp>
#include <praas/control-plane/worker.hpp>

#include <drogon/HttpAppFramework.h>
#include <drogon/HttpTypes.h>
#include <future>
#include <thread>

#include <spdlog/spdlog.h>

namespace praas::control_plane {

  HttpServer::HttpServer(config::HTTPServer & cfg, worker::Workers & workers):
    _port(cfg.port),
    _threads(cfg.threads),
    _workers(workers)
  {
    _logger = common::util::create_logger("HttpServer");
    drogon::app().setClientMaxBodySize(cfg.max_payload_size);
  }

  void HttpServer::run()
  {
    drogon::app().registerController(shared_from_this());
    drogon::app().setThreadNum(_threads);
    _server_thread = std::thread{
      [this]() {
        drogon::app().addListener("0.0.0.0", _port).run();
      }
    };
  }

  void HttpServer::shutdown()
  {
    _logger->info("Stopping HTTP server");
    drogon::app().getLoop()->queueInLoop([]() { drogon::app().quit(); });
    _server_thread.join();
    _logger->info("Stopped HTTP server");
  }

  drogon::HttpResponsePtr HttpServer::correct_response(const std::string& reason)
  {
    Json::Value json;
    json["message"] = reason;
    auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
    resp->setStatusCode(drogon::k200OK);
    return resp;
  }

  drogon::HttpResponsePtr HttpServer::failed_response(
    const std::string& reason, drogon::HttpStatusCode status_code
  )
  {
    Json::Value json;
    json["reason"] = reason;
    auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
    resp->setStatusCode(status_code);
    return resp;
  }

  void HttpServer::create_app(
    const drogon::HttpRequestPtr& request,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    std::string app_name
  )
  {
    // FIXME: store
    std::string container_name = request->getParameter("container_name");
    if(container_name.empty()) {
      callback(failed_response("Missing arguments!"));
    }

    _logger->info("Create new application {}", app_name);
    _workers.add_task(
      [callback=std::move(callback), app_name, this]() {
        if(_workers.create_application(app_name)) {
          callback(correct_response("Created"));
        } else {
          callback(correct_response("Failed to create"));
        }
      }
    );
  }

  void HttpServer::create_process(
    const drogon::HttpRequestPtr&,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    std::string app_id,
    std::string process_id
  )
  {
    //std::string proc_name = req->getParameter("name");
    //if(proc_name.empty()) {
    //  callback(failed_response("Missing arguments!"));
    //}

    //_logger->info("Create new process {}", proc_name);
    //_workers.add_task(
    //  &worker::Workers::create_process,
    //  request, std::move(callback), proc_name
    //);
    //if(_workers.create_application(app_name)) {
    //  callback(correct_response("Created"));
    //} else {
    //  callback(correct_response("Failed to create"));
    //}
  }

  void HttpServer::invoke(
    const drogon::HttpRequestPtr& request,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    std::string app_id, std::string fname
  )
  {
    if(app_id.empty() || fname.empty()) {
      callback(failed_response("Missing arguments!"));
    }

    _logger->info("Push new invocation request of {}", fname);
    _workers.add_task(
      &worker::Workers::handle_invocation,
      request, std::move(callback), app_id, fname
    );
  }

  void HttpServer::delete_process(
    const drogon::HttpRequestPtr&,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    std::string app_id,
    std::string process_id
  )
  {

  }

  void HttpServer::swap_process(
    const drogon::HttpRequestPtr&,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    std::string app_id,
    std::string process_id
  )
  {

  }

  void HttpServer::list_processes(
    const drogon::HttpRequestPtr&,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    std::string app_id
  )
  {

  }

  void HttpServer::list_apps(
    const drogon::HttpRequestPtr&,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
  )
  {

  }

} // namespace praas::control_plane
