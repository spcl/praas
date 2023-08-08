#include <praas/sdk/praas.hpp>

#include <future>

#include <drogon/HttpTypes.h>
#include <spdlog/fmt/fmt.h>

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

  std::optional<Process> PraaS::create_process(
      const std::string& application, const std::string& process_name, int vcpus, int memory
  )
  {
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Put);
    req->setPath(fmt::format("/apps/{}/processes/{}", application, process_name));
    req->setParameter("vcpus", std::to_string(vcpus));
    req->setParameter("memory", std::to_string(memory));

    std::promise<std::optional<Process>> p;

    _http_client->sendRequest(
        req,
        [&](drogon::ReqResult result, const drogon::HttpResponsePtr& response) {
          if (result == drogon::ReqResult::Ok) {
            auto& json_obj = *response->getJsonObject();
            auto ip_addr = json_obj["connection"]["ip-address"].asString();
            auto port = json_obj["connection"]["port"].asInt();
            p.set_value(Process{ip_addr, port, false});
          } else {
            p.set_value(std::nullopt);
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
            _last_error = (*json)["reason"].asString();
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

} // namespace praas::sdk
