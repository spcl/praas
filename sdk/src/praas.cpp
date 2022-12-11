#include <praas/sdk/praas.hpp>

#include <future>

#include <spdlog/fmt/fmt.h>

namespace praas::sdk {

  PraaS::PraaS(const std::string& control_plane_addr)
  {
    _loop.run();
    _http_client = drogon::HttpClient::newHttpClient(
      control_plane_addr, _loop.getLoop(), false, false
    );
  }

  void PraaS::disconnect()
  {
    _http_client.reset();
    _loop.getLoop()->quit();
    _loop.wait();
  }

  bool PraaS::create_application(const std::string & application)
  {
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath("/create_app");
    req->setParameter("name", application);

    std::promise<void> p;
    bool status = false;

    _http_client->sendRequest(req,
      [&](drogon::ReqResult, const drogon::HttpResponsePtr &response) {
        status = response->getStatusCode() == drogon::k200OK;
        p.set_value();
      }
    );
    p.get_future().wait();

    return status;

  }

  ControlPlaneInvocationResult PraaS::invoke(
    const std::string& app_name,
    const std::string& function_name,
    const std::string& invocation_data
  )
  {
    std::promise<void> p;
    ControlPlaneInvocationResult res;

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath(fmt::format("/invoke/{}/{}", app_name, function_name));
    req->setBody(invocation_data);
    req->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);

    _http_client->sendRequest(req,
      [&](drogon::ReqResult result, const drogon::HttpResponsePtr &response) {

        if( result != drogon::ReqResult::Ok || response->getStatusCode() != drogon::k200OK) {
          res.return_code = 1;
          auto json = response->getJsonObject();
          _last_error = (*json)["reason"].asString();
          p.set_value();
          return;
        }

        auto json = response->getJsonObject();
        res.invocation_id = (*json)["invocation_id"].asString();
        res.return_code = (*json)["return_code"].asInt();
        res.response = std::move((*json)["result"].asString());

        p.set_value();
      }
    );
    p.get_future().wait();

    return res;
  }

}
