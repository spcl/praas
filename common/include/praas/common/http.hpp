#ifndef PRAAS_COMMON_HTTP_HPP
#define PRAAS_COMMON_HTTP_HPP

#include <memory>
#include <string>

#include <drogon/HttpClient.h>
#include <drogon/HttpRequest.h>
#include <trantor/net/EventLoop.h>
#include <trantor/net/EventLoopThreadPool.h>

namespace praas::common::http {

  struct HTTPClient {

    using parameters_t = std::initializer_list<std::pair<std::string, std::string>>;

    HTTPClient(const std::string& address, trantor::EventLoop* loop);

    template <typename F>
    drogon::HttpRequestPtr put(const std::string& path, parameters_t&& params, F&& callback)
    {
      auto req = drogon::HttpRequest::newHttpRequest();
      req->setMethod(drogon::Put);
      req->setPath(path);
      request(req, std::forward<parameters_t>(params), std::forward<F>(callback));

      return req;
    }

    template <typename F>
    drogon::HttpRequestPtr post(const std::string& path, parameters_t&& params, F&& callback)
    {
      auto req = drogon::HttpRequest::newHttpRequest();
      req->setMethod(drogon::Post);
      req->setPath(path);
      request(req, std::forward<parameters_t>(params), std::forward<F>(callback));

      return req;
    }

    template <typename F>
    drogon::HttpRequestPtr get(const std::string& path, parameters_t&& params, F&& callback)
    {
      auto req = drogon::HttpRequest::newHttpRequest();
      req->setMethod(drogon::Get);
      req->setPath(path);
      request(req, std::forward<parameters_t>(params), std::forward<F>(callback));

      return req;
    }

  private:
    template <typename F>
    void request(drogon::HttpRequestPtr& req, parameters_t&& params, F&& callback)
    {
      for (const auto& param : params) {
        req->setParameter(param.first, param.second);
      }
      _http_client->sendRequest(req, callback);
    }

    drogon::HttpClientPtr _http_client;
  };

  struct HTTPClientFactory {

    static void initialize(int thread_num);
    static void shutdown();

    static HTTPClient create_client(std::string address, int port = -1);
    static HTTPClient create_client_shared(std::string address, int port = -1);

  private:
    static std::unique_ptr<trantor::EventLoopThreadPool> _pool;
    static trantor::EventLoop* _default_loop;
  };

} // namespace praas::common::http

#endif
