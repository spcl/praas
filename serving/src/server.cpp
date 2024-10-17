
#include <praas/common/http.hpp>
#include <praas/common/util.hpp>
#include <praas/serving/docker/containers.hpp>
#include <praas/serving/docker/server.hpp>

#include <drogon/HttpTypes.h>
#include <drogon/utils/Utilities.h>

#include <latch>
#include <string>

namespace praas::serving::docker {

  const std::string HttpServer::DEFAULT_SWAP_LOCATION = "/swaps";

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

            Json::Value resp;
            resp["status"] = fmt::format("Container for process {} created.", proc_name);
            resp["container-id"] = container_id;
            resp["process-id"] = proc_name;
            resp["ip-address"] = opts.server_ip;
            resp["port"] = std::stoi(port.asString());

            callback(common::http::HTTPClient::correct_response(resp));
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

            // Add waiting request to get response when the container is finished.

            auto client = common::http::HTTPClientFactory::create_client("http://127.0.0.1", opts.docker_port);
            auto req = client.post(
              fmt::format("/containers/{}/wait", container_id), {},
              [this, container_id, proc_name](drogon::ReqResult, const drogon::HttpResponsePtr& response) mutable {

                spdlog::info("Container for process {} exited!", proc_name);
                // sanity check - verify the container id agrees
                // we might have replaced the process with another one
                _processes.erase(proc_name, container_id);

              }
            );
            Process proc{
              proc_name, container_id,
              std::move(client), std::move(req)
            };
            _processes.add(proc_name, std::move(proc));

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
    //host_config["AutoRemove"] = true;
    host_config["AutoRemove"] = false;
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
      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
      const std::string& app, const std::string& process
  )
  {
    if (process.empty() || app.empty()) {
      callback(common::http::HTTPClient::failed_response("Missing arguments!"));
      return;
    }

    auto req_body = request->getJsonObject();
    if (req_body == nullptr) {
      callback(common::http::HTTPClient::failed_response("Missing body!"));
      return;
    }

    auto container_name_obj = (*req_body)["container-name"];
    auto controlplane_addr_obj = (*req_body)["controlplane-address"];
    auto swapinlocation = (*req_body)["swap-location"];
    auto s3_bucket = (*req_body)["s3-swapping-bucket"];
    if (container_name_obj.isNull() || controlplane_addr_obj.isNull()) {
      callback(common::http::HTTPClient::failed_response("Missing arguments in request body!"));
      return;
    }

    std::string container_name = container_name_obj.asString();
    std::string controlplane_addr = controlplane_addr_obj.asString();

    Json::Value body;
    body["Image"] = container_name;

    _configure_ports(body);

    // Configure swapping volume
    std::filesystem::path swaps_path = std::filesystem::absolute(opts.swaps_volume);
    Json::Value volumes;
    volumes.append(fmt::format("{}:{}:rw", swaps_path.string(), DEFAULT_SWAP_LOCATION));
    body["HostConfig"]["Binds"] = volumes;

    Json::Value env_data;
    env_data.append(fmt::format("CONTROLPLANE_ADDR={}", controlplane_addr));
    env_data.append(fmt::format("PROCESS_ID={}", process));
    env_data.append(fmt::format("SWAPS_LOCATION={}", DEFAULT_SWAP_LOCATION));
    if(!swapinlocation.isNull()) {
      env_data.append(fmt::format("SWAPIN_LOCATION={}", swapinlocation.asString()));
    }
    if(!s3_bucket.isNull()) {
      env_data.append(fmt::format("S3_SWAPPING_BUCKET={}", s3_bucket.asString()));
    }

    char* access_key = std::getenv("AWS_ACCESS_KEY_ID");
    char* secret_key = std::getenv("AWS_SECRET_ACCESS_KEY");
    if((access_key != nullptr) && (secret_key != nullptr)) {
      env_data.append(fmt::format("AWS_ACCESS_KEY_ID={}", access_key));
      env_data.append(fmt::format("AWS_SECRET_ACCESS_KEY={}", secret_key));
    }

    body["Env"] = env_data;

    std::string proc_name = Processes::name(app, process);

    _http_client.post(
        "/containers/create",
        {
            {"name", fmt::format("{}-{}", proc_name, _process_counter++)},
        },
        std::move(body),
        [callback = std::move(callback), this, proc_name](
          drogon::ReqResult result, const drogon::HttpResponsePtr& response
        ) mutable {
          if (response->getStatusCode() == drogon::HttpStatusCode::k409Conflict) {
            callback(common::http::HTTPClient::failed_response(
                fmt::format("Container for process {} already exists", proc_name)
            ));
          } else if (response->getStatusCode() == drogon::HttpStatusCode::k201Created) {

            auto container_id = (*response->getJsonObject())["Id"].asString();
            spdlog::debug("Created container {} for process {}.", container_id, proc_name);
            _start_container(proc_name, container_id, std::move(callback));
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
      const std::string& app_name, const std::string& process
  )
  {
    std::string proc_name = Processes::name(app_name, process);
    std::string container_id;
    {
      Processes::rw_acc_t acc;
      _processes.get(proc_name, acc);
      if (acc.empty()) {
        auto resp = common::http::HTTPClient::failed_response(
            fmt::format("Container for app {} process {} does not exist!", app_name, process)
        );
        resp->setStatusCode(drogon::k404NotFound);
        callback(resp);
      }
      container_id = acc->second.container_id;
    }

    _http_client.post(
        fmt::format("/containers/{}/stop", container_id), {{"signal", "SIGINT"}},
        [callback = std::move(callback), container_id, this,
         proc_name](drogon::ReqResult, const drogon::HttpResponsePtr& response) {
          if (response->getStatusCode() == drogon::HttpStatusCode::k404NotFound) {

            callback(common::http::HTTPClient::failed_response(fmt::format(
                "Failure - container {} for process {} does not exist", container_id, proc_name
            )));

          } else if (response->getStatusCode() == drogon::HttpStatusCode::k204NoContent) {

            _processes.erase(proc_name);
            callback(common::http::HTTPClient::correct_response(
                fmt::format("Container for process {} removed.", proc_name)
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
