#include <praas/sdk/praas.hpp>

#include <future>

#include <drogon/HttpTypes.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#include <thread>

#include <praas/common/http.hpp>

namespace praas::sdk {

  PraaS::PraaS(const std::string& control_plane_addr, int thread_num)
  {
    praas::common::http::HTTPClientFactory::initialize(thread_num);


    for(int i = 0; i < thread_num; ++i) {
      _clients.emplace(
        praas::common::http::HTTPClientFactory::create_client(control_plane_addr)
      );
    }
  }

  praas::common::http::HTTPClient PraaS::_get_client()
  {
    std::unique_lock<std::mutex> lock{_clients_mutex};

    if(_clients.empty()) {
      _cv.wait(lock);
    }

    auto client = std::move(_clients.front());
    _clients.pop();

    return client;
  }

  void PraaS::_return_client(praas::common::http::HTTPClient& client)
  {
    std::unique_lock<std::mutex> l{_clients_mutex};

    _clients.push(std::move(client));
  }

  void PraaS::disconnect()
  {}

  bool
  PraaS::create_application(const std::string& application, const std::string& cloud_resource_name)
  {
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Put);
    req->setPath(fmt::format("/apps/{}", application));
    req->setParameter("cloud_resource_name", cloud_resource_name);

    std::promise<bool> p;

    auto http_client = _get_client();
    http_client.handle()->sendRequest(
        req,
        [&](drogon::ReqResult result, const drogon::HttpResponsePtr& response) {
          if (result == drogon::ReqResult::Ok) {
            p.set_value(response->getStatusCode() == drogon::k200OK);
          } else {
            p.set_value(false);
          }
        }
    );
    _return_client(http_client);

    return p.get_future().get();
  }

  bool
  PraaS::get_application(const std::string& application)
  {
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath(fmt::format("/apps/{}", application));

    std::promise<bool> p;

    auto http_client = _get_client();
    http_client.handle()->sendRequest(
        req,
        [&](drogon::ReqResult result, const drogon::HttpResponsePtr& response) {
          if (result == drogon::ReqResult::Ok) {
            p.set_value(response->getStatusCode() == drogon::k200OK);
          } else {
            p.set_value(false);
          }
        }
    );
    _return_client(http_client);

    return p.get_future().get();
  }

  bool PraaS::delete_application(const std::string& application)
  {
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath(fmt::format("/apps/{}/delete", application));

    std::promise<bool> p;

    auto http_client = _get_client();
    http_client.handle()->sendRequest(
        req,
        [&](drogon::ReqResult result, const drogon::HttpResponsePtr& response) {
          if (result == drogon::ReqResult::Ok) {
            p.set_value(response->getStatusCode() == drogon::k200OK);
          } else {
            p.set_value(false);
          }
        }
    );
    _return_client(http_client);

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

    auto http_client = _get_client();
    http_client.handle()->sendRequest(
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
    _return_client(http_client);

    return p.get_future().get();
  }

  bool PraaS::stop_process(const Process& process)
  {
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath(fmt::format("/apps/{}/processes/{}/stop", process.app_name, process.process_id));

    std::promise<bool> p;

    auto http_client = _get_client();
    http_client.handle()->sendRequest(
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
    _return_client(http_client);

    return p.get_future().get();
  }

  ControlPlaneInvocationResult PraaS::invoke(
      const std::string& app_name, const std::string& function_name,
      const std::string& invocation_data, std::optional<std::string> process_name
  )
  {
    return invoke_async(app_name, function_name, invocation_data, process_name).get();
  }

  std::future<ControlPlaneInvocationResult> PraaS::invoke_async(
      const std::string& app_name, const std::string& function_name,
      const std::string& invocation_data, std::optional<std::string> process_name
  )
  {
    // We need a shared_ptr because we cannot move it to the lambda later
    // Drogon request requires a std::function which must be CopyConstructible
    auto p = std::make_shared<std::promise<ControlPlaneInvocationResult>>();
    auto fut = p->get_future();

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath(fmt::format("/apps/{}/invoke/{}", app_name, function_name));
    req->setBody(invocation_data);
    if(process_name.has_value()) {
      req->setParameter("process_name", process_name.value());
    }
    req->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);

    auto http_client = _get_client();
    http_client.handle()->sendRequest(
        req,
        [&, p = std::move(p)](drogon::ReqResult result, const drogon::HttpResponsePtr& response) mutable {

          ControlPlaneInvocationResult res;
          if (result != drogon::ReqResult::Ok || response->getStatusCode() != drogon::k200OK) {
            res.return_code = 1;

            if(response) {
              auto json = response->getJsonObject();
              res.error_message = (*json)["reason"].asString();
            } else {
              res.error_message = "request failed";
            }

            p->set_value(res);
            return;
          }

          auto json = response->getJsonObject();
          res.invocation_id = (*json)["invocation_id"].asString();
          res.process_name = (*json)["process_name"].asString();

          res.return_code = (*json)["return_code"].asInt();
          if (res.return_code < 0) {
            res.error_message = std::move((*json)["result"].asString());
          } else {
            res.response = std::move((*json)["result"].asString());
          }

          p->set_value(res);
        }
    );
    _return_client(http_client);

    return fut;
  }

  std::tuple<bool, std::string> PraaS::swap_process(const Process& process)
  {
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath(fmt::format("/apps/{}/processes/{}/swap", process.app_name, process.process_id));

    std::promise<std::tuple<bool, std::string>> p;

    auto http_client = _get_client();
    http_client.handle()->sendRequest(
        req,
        [&](drogon::ReqResult result, const drogon::HttpResponsePtr& response) {

          if (result == drogon::ReqResult::Ok && response->getStatusCode() == drogon::k200OK) {
            p.set_value(std::make_tuple(true, std::string{response->getBody()}));
          } else {
            p.set_value(std::make_tuple(false, std::string{response->getBody()}));
          }
        }
    );
    _return_client(http_client);

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

    auto http_client = _get_client();
    http_client.handle()->sendRequest(
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
    _return_client(http_client);

    return p.get_future().get();
  }

  bool PraaS::delete_process(const Process& process)
  {
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath(fmt::format("/apps/{}/processes/{}/delete", process.app_name, process.process_id));

    std::promise<bool> p;

    auto http_client = _get_client();
    http_client.handle()->sendRequest(
        req,
        [&](drogon::ReqResult result, const drogon::HttpResponsePtr& response) {

          if (result == drogon::ReqResult::Ok && response->getStatusCode() == drogon::k200OK) {
            p.set_value(true);
          } else {
            p.set_value(false);
          }
        }
    );
    _return_client(http_client);

    return p.get_future().get();
  }

} // namespace praas::sdk
