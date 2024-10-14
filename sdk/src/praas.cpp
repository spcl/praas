#include <praas/sdk/praas.hpp>

#include <future>

#include <drogon/HttpTypes.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

namespace praas::sdk {

  PraaS::PraaS(const std::string& control_plane_addr)
  {
    _loop.run();
    _http_client =
        drogon::HttpClient::newHttpClient(control_plane_addr, _loop.getLoop(), false, false);
  }

  void PraaS::disconnect()
  {
    _http_client.reset();
    _loop.getLoop()->quit();
    _loop.wait();
  }

  bool
  PraaS::create_application(const std::string& application, const std::string& cloud_resource_name)
  {
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Put);
    req->setPath(fmt::format("/apps/{}", application));
    req->setParameter("cloud_resource_name", cloud_resource_name);

    std::promise<bool> p;

    _http_client->sendRequest(
        req,
        [&](drogon::ReqResult result, const drogon::HttpResponsePtr& response) {
          if (result == drogon::ReqResult::Ok) {
            p.set_value(response->getStatusCode() == drogon::k200OK);
          } else {
            p.set_value(false);
          }
        }
    );
    return p.get_future().get();
  }

  bool PraaS::delete_application(const std::string& application)
  {
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath(fmt::format("/apps/{}/delete", application));

    std::promise<bool> p;

    _http_client->sendRequest(
        req,
        [&](drogon::ReqResult result, const drogon::HttpResponsePtr& response) {
          if (result == drogon::ReqResult::Ok) {
            p.set_value(response->getStatusCode() == drogon::k200OK);
          } else {
            p.set_value(false);
          }
        }
    );
    return p.get_future().get();

  }

  std::optional<Process> PraaS::create_process(
      const std::string& application, const std::string& process_name, std::string vcpus,
      std::string memory
  )
  {
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Put);
    req->setPath(fmt::format("/apps/{}/processes/{}", application, process_name));
    req->setParameter("vcpus", vcpus);
    req->setParameter("memory", memory);

    std::promise<std::optional<Process>> p;

    _http_client->sendRequest(
        req,
        [&](drogon::ReqResult result, const drogon::HttpResponsePtr& response) {
          spdlog::info("Received callback Created process");
          if (result == drogon::ReqResult::Ok && response->getStatusCode() == drogon::k200OK) {
            auto& json_obj = *response->getJsonObject();
            auto ip_addr = json_obj["connection"]["ip-address"].asString();
            auto port = json_obj["connection"]["port"].asInt();
            p.set_value(Process{application, process_name, ip_addr, port, true});
          } else {
            std::cerr << *response->getJsonObject() << std::endl;
            p.set_value(std::nullopt);
          }
        }
    );
    return p.get_future().get();
  }

  bool PraaS::stop_process(const Process& process)
  {
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath(fmt::format("/apps/{}/processes/{}/stop", process.app_name, process.process_id));

    std::promise<bool> p;

    _http_client->sendRequest(
        req,
        [&](drogon::ReqResult result, const drogon::HttpResponsePtr& response) {

          if (result == drogon::ReqResult::Ok && response->getStatusCode() == drogon::k200OK) {
            p.set_value(true);
          } else {
            std::cerr << *response->getJsonObject() << std::endl;
            p.set_value(false);
          }
        }
    );
    return p.get_future().get();
  }

  ControlPlaneInvocationResult PraaS::invoke(
      const std::string& app_name, const std::string& function_name,
      const std::string& invocation_data
  )
  {
    std::promise<void> p;
    ControlPlaneInvocationResult res;

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath(fmt::format("/apps/{}/invoke/{}", app_name, function_name));
    req->setBody(invocation_data);
    req->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);

    _http_client->sendRequest(
        req,
        [&](drogon::ReqResult result, const drogon::HttpResponsePtr& response) {
          if (result != drogon::ReqResult::Ok || response->getStatusCode() != drogon::k200OK) {
            res.return_code = 1;
            auto json = response->getJsonObject();
            res.error_message = (*json)["reason"].asString();

            p.set_value();
            return;
          }

          auto json = response->getJsonObject();
          res.invocation_id = (*json)["invocation_id"].asString();

          res.return_code = (*json)["return_code"].asInt();
          if (res.return_code < 0) {
            res.error_message = std::move((*json)["result"].asString());
          } else {
            res.response = std::move((*json)["result"].asString());
          }

          p.set_value();
        }
    );
    p.get_future().wait();

    return res;
  }

  std::optional<std::string> PraaS::swap_process(const Process& process)
  {
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath(fmt::format("/apps/{}/processes/{}/swap", process.app_name, process.process_id));

    std::promise<std::optional<std::string>> p;

    _http_client->sendRequest(
        req,
        [&](drogon::ReqResult result, const drogon::HttpResponsePtr& response) {

          if (result == drogon::ReqResult::Ok && response->getStatusCode() == drogon::k200OK) {
            p.set_value(std::make_optional<std::string>(response->getBody()));
          } else {
            p.set_value(std::nullopt);
          }
        }
    );
    return p.get_future().get();
  }

  std::optional<Process> PraaS::swapin_process(
    const std::string& application, const std::string& process_name
  )
  {
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath(fmt::format("/apps/{}/processes/{}/swapin", application, process_name));

    std::promise<std::optional<Process>> p;

    _http_client->sendRequest(
        req,
        [&](drogon::ReqResult result, const drogon::HttpResponsePtr& response) {

          if (result == drogon::ReqResult::Ok && response->getStatusCode() == drogon::k200OK) {
            auto& json_obj = *response->getJsonObject();
            auto ip_addr = json_obj["connection"]["ip-address"].asString();
            auto port = json_obj["connection"]["port"].asInt();
            p.set_value(Process{application, process_name, ip_addr, port, true});
          } else {
            spdlog::error("Failed to swap in process!");
            std::cerr << *response->getJsonObject() << std::endl;
            p.set_value(std::nullopt);
          }
        }
    );
    return p.get_future().get();
  }

  bool PraaS::delete_process(const Process& process)
  {
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath(fmt::format("/apps/{}/processes/{}/delete", process.app_name, process.process_id));

    std::promise<bool> p;

    _http_client->sendRequest(
        req,
        [&](drogon::ReqResult result, const drogon::HttpResponsePtr& response) {

          if (result == drogon::ReqResult::Ok && response->getStatusCode() == drogon::k200OK) {
            p.set_value(true);
          } else {
            p.set_value(false);
          }
        }
    );
    return p.get_future().get();
  }

} // namespace praas::sdk
