#ifndef PRAAS_SDK_PRAAS_HPP
#define PRAAS_SDK_PRAAS_HPP

#include <praas/sdk/invocation.hpp>

#include <drogon/HttpClient.h>
#include <trantor/net/EventLoopThread.h>

namespace praas::sdk {

  struct PraaS {

    PraaS(const std::string& control_plane_addr);

    void disconnect();

    bool create_application(const std::string & application);

  private:

    trantor::EventLoopThread _loop;

    drogon::HttpClientPtr _http_client;
  };

}

#endif
