#ifndef PRAAS_COMMON_HTTP_HPP
#define PRAAS_COMMON_HTTP_HPP

#include <functional>
#include <memory>
#include <string>

namespace drogon {
  struct HttpRequest;
  struct HttpResponse;
  enum class ReqResult;
  struct HttpClient;
} // namespace drogon

namespace trantor {
  struct EventLoop;
  struct EventLoopThreadPool;
} // namespace trantor

namespace praas::common::http {

  struct HTTPClient {

    using parameters_t = std::initializer_list<std::pair<std::string, std::string>>;
    using callback_t =
        std::function<void(drogon::ReqResult, const std::shared_ptr<drogon::HttpResponse>&)>;

    HTTPClient();

    HTTPClient(const std::string& address, trantor::EventLoop* loop);

    std::shared_ptr<drogon::HttpRequest>
    put(const std::string& path, parameters_t&& params, callback_t&& callback);

    std::shared_ptr<drogon::HttpRequest>
    get(const std::string& path, parameters_t&& params, callback_t&& callback);

    std::shared_ptr<drogon::HttpRequest>
    post(const std::string& path, parameters_t&& params, callback_t&& callback);

  private:
    void request(
        std::shared_ptr<drogon::HttpRequest>& req, parameters_t&& params, callback_t&& callback
    );

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
