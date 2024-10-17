#ifndef PRAAS_SDK_PRAAS_HPP
#define PRAAS_SDK_PRAAS_HPP

#include <praas/sdk/invocation.hpp>
#include <praas/sdk/process.hpp>

#include <condition_variable>
#include <drogon/HttpClient.h>
#include <trantor/net/EventLoopThread.h>
#include "praas/common/http.hpp"

namespace praas::sdk {

  struct PraaS {

    PraaS(const std::string& control_plane_addr, int thread_num = 8);

    void disconnect();

    bool create_application(const std::string& application, const std::string& cloud_resource_name);

    bool get_application(const std::string& application);

    bool delete_application(const std::string& application);

    std::optional<Process> create_process(
      const std::string& application, const std::string& process_name,
      const std::string& vcpus, const std::string& memory
    );

    std::future<std::optional<Process>> create_process_async(
      const std::string& application, const std::string& process_name,
      const std::string& vcpus, const std::string& memory
    );

    ControlPlaneInvocationResult invoke(
        const std::string& app_name, const std::string& function_name,
        const std::string& invocation_data,
        std::optional<std::string> process_name = std::nullopt
    );

    std::future<ControlPlaneInvocationResult> invoke_async(
        const std::string& app_name, const std::string& function_name,
        const std::string& invocation_data,
        std::optional<std::string> process_name = std::nullopt
    );

    std::tuple<bool, std::string> swap_process(const Process& process);

    std::optional<Process> swapin_process(
      const std::string& application, const std::string& process_name
    );

    bool delete_process(const Process& process);

    bool delete_process(std::string_view app_name, std::string_view process_id);

    bool stop_process(const Process& process);

    bool stop_process(std::string_view app_name, std::string_view process_id);

    std::string_view last_error() const;

  private:
    std::string _last_error;

    std::mutex _clients_mutex;
    std::condition_variable _cv;
    std::queue<common::http::HTTPClient> _clients;

    praas::common::http::HTTPClient _get_client();
    void _return_client(praas::common::http::HTTPClient& client);
  };

} // namespace praas::sdk

#endif
