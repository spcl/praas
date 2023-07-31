#include <praas/common/http.hpp>

#include <praas/common/exceptions.hpp>

#include <drogon/HttpClient.h>
#include <drogon/HttpRequest.h>
#include <fmt/format.h>
#include <json/value.h>
#include <trantor/net/EventLoop.h>
#include <trantor/net/EventLoopThreadPool.h>

namespace praas::common::http {

  std::unique_ptr<trantor::EventLoopThreadPool> HTTPClientFactory::_pool = nullptr;
  trantor::EventLoop* HTTPClientFactory::_default_loop = nullptr;

  HTTPClient::HTTPClient() : _http_client(nullptr) {}

  HTTPClient::HTTPClient(const std::string& address, trantor::EventLoop* loop)
  {
    this->_http_client = drogon::HttpClient::newHttpClient(address, loop, false, false);
  }

  std::shared_ptr<drogon::HttpRequest>
  HTTPClient::put(const std::string& path, parameters_t&& params, callback_t&& callback)
  {
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Put);
    req->setPath(path);
    request(req, std::forward<parameters_t>(params), std::forward<callback_t>(callback));

    return req;
  }

  std::shared_ptr<drogon::HttpRequest>
  HTTPClient::post(const std::string& path, parameters_t&& params, callback_t&& callback)
  {
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath(path);
    request(req, std::forward<parameters_t>(params), std::forward<callback_t>(callback));

    return req;
  }

  std::shared_ptr<drogon::HttpRequest> HTTPClient::post(
      const std::string& path, parameters_t&& params, Json::Value&& body, callback_t&& callback
  )
  {
    auto req = drogon::HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath(path);
    request(req, std::forward<parameters_t>(params), std::forward<callback_t>(callback));

    return req;
  }

  std::shared_ptr<drogon::HttpRequest>
  HTTPClient::get(const std::string& path, parameters_t&& params, callback_t&& callback)
  {
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath(path);
    request(req, std::forward<parameters_t>(params), std::forward<callback_t>(callback));

    return req;
  }

  void HTTPClient::request(
      std::shared_ptr<drogon::HttpRequest>& req, parameters_t&& params, callback_t&& callback
  )
  {
    for (const auto& param : params) {
      req->setParameter(param.first, param.second);
    }
    _http_client->sendRequest(req, callback);
  }

  HTTPClient::response_ptr_t HTTPClient::correct_response(const std::string& reason)
  {
    Json::Value json;
    json["status"] = reason;
    auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
    resp->setStatusCode(drogon::k200OK);
    return resp;
  }

  HTTPClient::response_ptr_t HTTPClient::correct_response(const Json::Value& response)
  {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(drogon::k200OK);
    return resp;
  }

  HTTPClient::response_ptr_t HTTPClient::failed_response(const std::string& reason)
  {
    Json::Value json;
    json["reason"] = reason;
    auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
    resp->setStatusCode(drogon::HttpStatusCode::k500InternalServerError);
    return resp;
  }

  void HTTPClientFactory::initialize(int thread_num)
  {
    HTTPClientFactory::_pool = std::make_unique<trantor::EventLoopThreadPool>(thread_num);
    HTTPClientFactory::_pool->start();
    HTTPClientFactory::_default_loop = HTTPClientFactory::_pool->getNextLoop();
  }

  void HTTPClientFactory::shutdown()
  {
    HTTPClientFactory::_pool.reset();
  }

  HTTPClient HTTPClientFactory::create_client(std::string address, int port)
  {
    if (!_pool) {
      throw common::PraaSException("Uninitialized HTTPClientFactory!");
    }

    if (port != -1) {
      return HTTPClient{fmt::format("{}:{}", address, port), _pool->getNextLoop()};
    } else {
      return HTTPClient{address, _pool->getNextLoop()};
    }
  }

  HTTPClient HTTPClientFactory::create_client_shared(std::string address, int port)
  {
    if (!_default_loop) {
      throw common::PraaSException("Uninitialized HTTPClientFactory!");
    }

    if (port != -1) {
      return HTTPClient{fmt::format("{}:{}", address, port), _default_loop};
    } else {
      return HTTPClient{address, _default_loop};
    }
  }

} // namespace praas::common::http
