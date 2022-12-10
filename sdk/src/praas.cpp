#include <praas/sdk/praas.hpp>

#include <future>

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

}
