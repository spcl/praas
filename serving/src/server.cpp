
#include <praas/common/http.hpp>
#include <praas/common/util.hpp>
#include <praas/serving/docker/server.hpp>

#include <drogon/HttpTypes.h>
#include <drogon/utils/Utilities.h>

namespace praas::serving::docker {

  HttpServer::HttpServer(Options& cfg)
      : _http_port(cfg.http_port), _docker_port(cfg.docker_port), _threads(cfg.threads),
        _max_processes(cfg.max_processes)
  {
    _logger = common::util::create_logger("HttpServer");
  }

  void HttpServer::start()
  {
    _logger->info("Starting HTTP server");
    drogon::app().registerController(shared_from_this());
    drogon::app().setThreadNum(_threads);

    _http_client = common::http::HTTPClientFactory::create_client("http://127.0.0.1", _docker_port);

    _server_thread =
        std::thread{[this]() { drogon::app().addListener("0.0.0.0", _http_port).run(); }};
  }

  void HttpServer::shutdown()
  {
    _logger->info("Stopping HTTP server");
    drogon::app().getLoop()->queueInLoop([]() { drogon::app().quit(); });
    _server_thread.join();
    _logger->info("Stopped HTTP server");
  }

  void HttpServer::wait()
  {
    _server_thread.join();
  }

  std::vector<std::string_view> split(std::string_view string, const std::string& delimiter)
  {
    std::vector<std::string_view> substrings;

    size_t cur = 0;
    size_t prev = 0;
    while ((cur = string.find(delimiter, prev)) != std::string::npos) {
      substrings.emplace_back(string.data() + prev, cur - prev);
      prev = cur + delimiter.size();
    }

    if (string.length() - prev > 0) {
      substrings.emplace_back(string.data() + prev, string.length() - prev);
    }

    return substrings;
  }

  drogon::HttpResponsePtr correct_response(const std::string& reason)
  {
    Json::Value json;
    json["status"] = reason;
    auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
    resp->setStatusCode(drogon::k200OK);
    return resp;
  }

  drogon::HttpResponsePtr failed_response(
      const std::string& reason,
      drogon::HttpStatusCode status_code = drogon::HttpStatusCode::k500InternalServerError
  )
  {
    Json::Value json;
    json["reason"] = reason;
    auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
    resp->setStatusCode(status_code);
    return resp;
  }

  void HttpServer::_start_container(
      const std::string& proc_name, const std::string& container_id,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback
  )
  {
    _http_client.post(
        fmt::format("/containers/{}/start", container_id), {},
        [callback = std::move(callback), container_id, this,
         proc_name](drogon::ReqResult, const drogon::HttpResponsePtr& response) {
          if (response->getStatusCode() == drogon::HttpStatusCode::k404NotFound) {
            callback(failed_response(fmt::format(
                "Failure - container {} for process {} no longer exists", container_id, proc_name
            )));
          } else if (response->getStatusCode() == drogon::HttpStatusCode::k204NoContent) {

            _processes.add(proc_name, Process{proc_name, container_id});
            callback(correct_response(fmt::format("Container for process {} created.", proc_name)));
          } else {
            callback(failed_response(fmt::format("Unknown error! Response: {}", response->getBody())
            ));
          }
        }
    );
  }

  void HttpServer::create(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback
  )
  {
    std::string proc_name = request->getParameter("process-name");
    if (proc_name.empty()) {
      callback(failed_response("Missing arguments!"));
      return;
    }

    auto req_body = request->getJsonObject();
    if (req_body == nullptr) {
      callback(failed_response("Missing body!"));
      return;
    }

    auto container_name_obj = (*req_body)["container-name"];
    auto controlplane_addr_obj = (*req_body)["controlplane-address"];
    if (container_name_obj.isNull() || controlplane_addr_obj.isNull()) {
      callback(failed_response("Missing arguments in request body!"));
      return;
    }
    std::string container_name = container_name_obj.asString();
    std::string controlplane_addr = controlplane_addr_obj.asString();

    Json::Value body;
    body["Image"] = container_name;

    Json::Value env_data;
    // FIXME: reenable once Docker is enabled to handle empty strings for addr
    // env_data.append(fmt::format("CONTROLPLANE_ADDR={}", controlplane_addr));
    env_data.append(fmt::format("PROCESS_ID={}", proc_name));
    body["Env"] = env_data;

    // FIXME: volumes
    // FIXME: port mapping

    _http_client.post(
        "/containers/create",
        {
            {"name", proc_name},
        },
        std::move(body),
        [callback = std::move(callback), this,
         proc_name](drogon::ReqResult result, const drogon::HttpResponsePtr& response) mutable {
          if (response->getStatusCode() == drogon::HttpStatusCode::k409Conflict) {
            callback(
                failed_response(fmt::format("Container for process {} already exists", proc_name))
            );
          } else if (response->getStatusCode() == drogon::HttpStatusCode::k201Created) {

            auto container_id = (*response->getJsonObject())["Id"].asString();
            _start_container(proc_name, container_id, std::move(callback));
          } else {
            callback(failed_response(fmt::format("Unknown error! Response: {}", response->getBody())
            ));
          }
        }
    );
  }

  void HttpServer::kill(
      const drogon::HttpRequestPtr&, std::function<void(const drogon::HttpResponsePtr&)>&& callback
  )
  {
  }

  void HttpServer::cache_image(
      const drogon::HttpRequestPtr&, std::function<void(const drogon::HttpResponsePtr&)>&& callback,
      std::string image
  )
  {
    // auto url = fmt::format("/images/create?fromImage={}", image);
    auto url = "/images/create";
    _http_client.post(
        url, {{"fromImage", image}},
        [callback, image](drogon::ReqResult, const drogon::HttpResponsePtr& response) {
          Json::Value resp_json;
          resp_json["image"] = image;
          drogon::HttpStatusCode code = drogon::kUnknown;

          auto strings = split(response->getBody(), "\n");
          Json::Value val;
          Json::Reader reader;
          // Parse the last received JSON
          bool status = reader.parse(strings.back().begin(), strings.back().end(), val, false);
          if (status) {

            if (val["status"].asString().find("Image is up to date") != std::string::npos) {
              resp_json["status"] = val["status"];
              code = drogon::k200OK;
            } else if (val["message"].asString().find("manifest unknown") != std::string::npos) {
              resp_json["status"] = val["message"];
              code = drogon::k404NotFound;
            } else {
              resp_json["status"] = val["message"];
              code = drogon::k500InternalServerError;
            }

          } else {
            resp_json["status"] =
                fmt::format("Couldn't parse the output JSON! JSON: {}", strings.back());
            code = drogon::k500InternalServerError;
          }

          auto resp = drogon::HttpResponse::newHttpJsonResponse(resp_json);
          resp->setStatusCode(code);
          callback(resp);
        }
    );
  }

} // namespace praas::serving::docker
