#ifndef PRAAS_SDK_PRAAS_HPP
#define PRAAS_SDK_PRAAS_HPP

#include <praas/sdk/invocation.hpp>

#include <drogon/HttpClient.h>
#include <trantor/net/EventLoopThread.h>

namespace praas::sdk {

  struct PraaS {

    PraaS(const std::string& control_plane_addr);

    void disconnect();

    bool create_application(const std::string& application, const std::string& cloud_resource_name);

    ControlPlaneInvocationResult invoke(
        const std::string& app_name, const std::string& function_name,
        const std::string& invocation_data
    );

    std::string_view last_error() const;

  private:
    std::string _last_error;

    trantor::EventLoopThread _loop;

    drogon::HttpClientPtr _http_client;
  };

} // namespace praas::sdk

#endif
