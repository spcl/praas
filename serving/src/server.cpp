
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

  void HttpServer::create(
      const drogon::HttpRequestPtr&, std::function<void(const drogon::HttpResponsePtr&)>&& callback
  )
  {
    std::cerr << drogon::utils::urlEncode("/images/create?fromImage=python:3.7-slim-stretch")
              << std::endl;
    std::cerr << drogon::utils::urlEncode("/images/create?fromImage=\"python:3.7-slim-stretch\"")
              << std::endl;
    // auto img = drogon::utils::urlEncode("/images/create?fromImage=python:3.7-slim-stretch");
    auto img = "/images/create?fromImage=python:3.7-slim-stretch";
    _http_client.post(
        "/images/create?fromImage=python:3.7-slim-stretch", {},
        [callback](drogon::ReqResult result, const drogon::HttpResponsePtr& response) {
          auto strings = split(response->getBody(), "\n");
          Json::Value val;
          Json::Reader reader;
          // Parse the last received JSON
          bool status = reader.parse(strings.back().begin(), strings.back().end(), val, false);
          if (status) {
            auto resp = drogon::HttpResponse::newHttpJsonResponse(val);
            resp->setStatusCode(drogon::k200OK);
            callback(resp);
          } else {
            Json::Value val;
            val["status"] = "Couldn't parse the output JSON!" + std::string{strings.back()};
            auto resp = drogon::HttpResponse::newHttpJsonResponse(val);
            resp->setStatusCode(drogon::k500InternalServerError);
            callback(resp);
          }
        }
    );
  }

  void HttpServer::swap(
      const drogon::HttpRequestPtr&, std::function<void(const drogon::HttpResponsePtr&)>&& callback
  )
  {
  }

  void HttpServer::cache_image(
      const drogon::HttpRequestPtr&, std::function<void(const drogon::HttpResponsePtr&)>&& callback,
      std::string image
  )
  {
    auto url = fmt::format("/images/create?fromImage={}", image);
    _http_client.post(
        url, {},
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
