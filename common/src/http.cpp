
#include <praas/common/http.hpp>

#include <fmt/format.h>
#include <trantor/net/EventLoopThreadPool.h>

namespace praas::common::http {

  std::unique_ptr<trantor::EventLoopThreadPool> HTTPClientFactory::_pool;
  trantor::EventLoop* HTTPClientFactory::_default_loop;

  HTTPClient::HTTPClient() : _http_client(nullptr) {}

  HTTPClient::HTTPClient(const std::string& address, trantor::EventLoop* loop)
  {
    this->_http_client = drogon::HttpClient::newHttpClient(address, loop, false, false);
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
    if (port != -1) {
      return HTTPClient{fmt::format("{}:{}", address, port), _pool->getNextLoop()};
    } else {
      return HTTPClient{address, _pool->getNextLoop()};
    }
  }

  HTTPClient HTTPClientFactory::create_client_shared(std::string address, int port)
  {
    if (port != -1) {
      return HTTPClient{fmt::format("{}:{}", address, port), _default_loop};
    } else {
      return HTTPClient{address, _default_loop};
    }
  }

} // namespace praas::common::http
