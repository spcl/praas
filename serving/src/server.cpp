
#include <praas/common/http.hpp>
#include <praas/common/util.hpp>
#include <praas/serving/docker/containers.hpp>
#include <praas/serving/docker/server.hpp>

#include <drogon/HttpTypes.h>
#include <drogon/utils/Utilities.h>

#include <latch>
#include <string>

namespace praas::serving::docker {

  HttpServer::HttpServer(Options& cfg) : opts(cfg)
  {
    _logger = common::util::create_logger("HttpServer");
  }

  void HttpServer::start()
  {
    _logger->info("Starting HTTP server at port {}", opts.http_port);
    drogon::app().registerController(shared_from_this());
    drogon::app().setThreadNum(opts.threads);

    _http_client =
        common::http::HTTPClientFactory::create_client("http://127.0.0.1", opts.docker_port);

    _server_thread =
        std::thread{[this]() { drogon::app().addListener("0.0.0.0", opts.http_port).run(); }};
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
    _kill_all();
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

  void HttpServer::_inspect_container(
      const std::string& proc_name, const std::string& container_id,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback
  )
  {
    _http_client.get(
        fmt::format("/containers/{}/json", container_id), {},
        [callback = std::move(callback), container_id, this,
         proc_name](drogon::ReqResult, const drogon::HttpResponsePtr& response) {
          if (response->getStatusCode() == drogon::HttpStatusCode::k200OK) {

            auto& response_json = *response->getJsonObject();
            auto& port = response_json["NetworkSettings"]["Ports"]
                                      [fmt::format("{}/tcp", opts.process_port)][0]["HostPort"];

            Json::Value response;
            response["status"] = fmt::format("Container for process {} created.", proc_name);
            response["container-id"] = container_id;
            response["process-id"] = proc_name;
            response["ip-address"] = opts.server_ip;
            response["port"] = std::stoi(port.asString());
            callback(common::http::HTTPClient::correct_response(response));
          } else {
            callback(common::http::HTTPClient::failed_response(
                fmt::format("Unknown error! Response: {}", response->getBody())
            ));
          }
        }
    );
  }

  void HttpServer::_start_container(
      const std::string& proc_name, const std::string& container_id,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback
  )
  {
    _http_client.post(
        fmt::format("/containers/{}/start", container_id), {},
        [callback = std::move(callback), container_id, this,
         proc_name](drogon::ReqResult, const drogon::HttpResponsePtr& response) mutable {
          if (response->getStatusCode() == drogon::HttpStatusCode::k404NotFound) {
            callback(common::http::HTTPClient::failed_response(fmt::format(
                "Failure - container {} for process {} no longer exists", container_id, proc_name
            )));
          } else if (response->getStatusCode() == drogon::HttpStatusCode::k204NoContent) {

            _processes.add(proc_name, Process{proc_name, container_id});
            spdlog::debug("Started container {} for process {}.", container_id, proc_name);
            _inspect_container(proc_name, container_id, std::move(callback));

          } else {
            callback(common::http::HTTPClient::failed_response(
                fmt::format("Unknown error! Response: {}", response->getBody())
            ));
          }
        }
    );
  }

  void HttpServer::_configure_ports(Json::Value& body)
  {
    Json::Value host_port;
    Json::Value bind_ports;
    host_port["HostIp"] = "0.0.0.0";
    bind_ports.append(host_port);

    Json::Value port_bindings;
    port_bindings[fmt::format("{}/tcp", opts.process_port)] = bind_ports;
    Json::Value host_config;
    host_config["PortBindings"] = port_bindings;
    host_config["AutoRemove"] = true;
    //host_config["NetworkMode"] = "bridge";

    Json::Value extra_hosts;
    extra_hosts.append("host.docker.internal:host-gateway");
    //host_config["NetworkMode"] = "bridge";
    //host_config["ExtraHosts"] = extra_hosts;
    body["HostConfig"] = host_config;

    // Docker documentation here is lacking details.
    // It is not sufficient ot create PortBindings.
    // We also need ExposedPorts.
    // Otherwise, ports will not be reachable from host.
    Json::Value exposed_ports;
    exposed_ports[fmt::format("{}/tcp", opts.process_port)] = Json::Value{};
    body["ExposedPorts"] = exposed_ports;
  }

  void HttpServer::create(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback, const std::string& process
  )
  {
    if (process.empty()) {
      callback(common::http::HTTPClient::failed_response("Missing arguments!"));
      return;
    }

    auto req_body = request->getJsonObject();
    if (req_body == nullptr) {
      callback(common::http::HTTPClient::failed_response("Missing body!"));
      return;
    }

    // TODO: add swap location argument
    auto container_name_obj = (*req_body)["container-name"];
    auto controlplane_addr_obj = (*req_body)["controlplane-address"];
    if (container_name_obj.isNull() || controlplane_addr_obj.isNull()) {
      callback(common::http::HTTPClient::failed_response("Missing arguments in request body!"));
      return;
    }
    std::string container_name = container_name_obj.asString();
    std::string controlplane_addr = controlplane_addr_obj.asString();

    Json::Value body;
    body["Image"] = container_name;

    Json::Value env_data;
    env_data.append(fmt::format("CONTROLPLANE_ADDR={}", controlplane_addr));
    env_data.append(fmt::format("PROCESS_ID={}", process));
    body["Env"] = env_data;
    _configure_ports(body);

    // FIXME: volumes

    _http_client.post(
        "/containers/create",
        {
            {"name", process},
        },
        std::move(body),
        [callback = std::move(callback), this,
         process](drogon::ReqResult result, const drogon::HttpResponsePtr& response) mutable {
          if (response->getStatusCode() == drogon::HttpStatusCode::k409Conflict) {
            callback(common::http::HTTPClient::failed_response(
                fmt::format("Container for process {} already exists", process)
            ));
          } else if (response->getStatusCode() == drogon::HttpStatusCode::k201Created) {

            auto container_id = (*response->getJsonObject())["Id"].asString();
            spdlog::debug("Created container {} for process {}.", container_id, process);
            _start_container(process, container_id, std::move(callback));
          } else {
            callback(common::http::HTTPClient::failed_response(
                fmt::format("Unknown error! Response: {}", response->getBody())
            ));
          }
        }
    );
  }

  void HttpServer::_kill_all()
  {
    std::vector<Process> processes;
    _processes.get_all(processes);

    std::latch all_killed{processes.size()};

    for (Process& proc : processes) {
      _http_client.post(
          fmt::format("/containers/{}/stop", proc.container_id), {{"signal", "SIGINT"}},
          [&all_killed](drogon::ReqResult, const drogon::HttpResponsePtr& response) {
            all_killed.count_down();
          }
      );
    }

    all_killed.wait();
  }

  void HttpServer::kill(
      const drogon::HttpRequestPtr&, std::function<void(const drogon::HttpResponsePtr&)>&& callback,
      const std::string& process
  )
  {
    std::string container_id;
    {
      Processes::rw_acc_t acc;
      _processes.get(process, acc);
      if (acc.empty()) {
        auto resp = common::http::HTTPClient::failed_response(
            fmt::format("Container for process {} does not exist!", process)
        );
        resp->setStatusCode(drogon::k404NotFound);
        callback(resp);
      }
      container_id = acc->second.container_id;
    }

    _http_client.post(
        fmt::format("/containers/{}/stop", container_id), {{"signal", "SIGINT"}},
        [callback = std::move(callback), container_id, this,
         process](drogon::ReqResult, const drogon::HttpResponsePtr& response) {
          if (response->getStatusCode() == drogon::HttpStatusCode::k404NotFound) {

            callback(common::http::HTTPClient::failed_response(fmt::format(
                "Failure - container {} for process {} does not exist", container_id, process
            )));

          } else if (response->getStatusCode() == drogon::HttpStatusCode::k204NoContent) {

            _processes.erase(process);
            callback(common::http::HTTPClient::correct_response(
                fmt::format("Container for process {} removed.", process)
            ));

          } else {
            callback(common::http::HTTPClient::failed_response(
                fmt::format("Unknown error! Response: {}", response->getBody())
            ));
          }
        }
    );
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

  void HttpServer::list_containers(
      const drogon::HttpRequestPtr&,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback
  )
  {
    std::vector<Process> processes;
    _processes.get_all(processes);

    Json::Value containers = Json::arrayValue;
    for(auto & p : processes) {
      Json::Value val;
      val["process"] = p.process_id;
      val["container"] = p.container_id;
      containers.append(val);
    }

    Json::Value resp_json;
    resp_json["processes"] = containers;
    auto resp = drogon::HttpResponse::newHttpJsonResponse(resp_json);
    resp->setStatusCode(drogon::k200OK);
    callback(resp);
  }

} // namespace praas::serving::docker
