#ifndef PRAAS_COMMON_HTTP_HPP
#define PRAAS_COMMON_HTTP_HPP

#include <functional>
#include <memory>
#include <string>

namespace drogon {
  struct HttpRequest;
  struct HttpResponse;
  struct HttpResponse;
  enum class ReqResult;
  struct HttpClient;
} // namespace drogon

namespace trantor {
  struct EventLoop;
  struct EventLoopThreadPool;
} // namespace trantor

namespace Json {
  struct Value;
} // namespace Json

namespace praas::common::http {

  struct HTTPClient {

    using request_ptr_t = std::shared_ptr<drogon::HttpRequest>;
    using response_ptr_t = std::shared_ptr<drogon::HttpResponse>;
    using parameters_t = std::initializer_list<std::pair<std::string, std::string>>;
    using callback_t =
        std::function<void(drogon::ReqResult, const std::shared_ptr<drogon::HttpResponse>&)>;

    HTTPClient();

    HTTPClient(const std::string& address, trantor::EventLoop* loop);

    request_ptr_t put(const std::string& path, parameters_t&& params, callback_t&& callback);

    request_ptr_t get(const std::string& path, parameters_t&& params, callback_t&& callback);

    request_ptr_t post(const std::string& path, parameters_t&& params, callback_t&& callback);

    request_ptr_t
    post(const std::string& path, parameters_t&& params, Json::Value&& body, callback_t&& callback);

    static response_ptr_t correct_response(const std::string& response);

    static response_ptr_t correct_response(const Json::Value& response);

    static response_ptr_t failed_response(const std::string& reason);

  private:
    void request(request_ptr_t& req, parameters_t&& params, callback_t&& callback);

    std::shared_ptr<drogon::HttpClient> _http_client;
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
