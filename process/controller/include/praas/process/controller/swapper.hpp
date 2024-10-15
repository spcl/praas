#ifndef PRAAS_PROCESS_CONTROLLER_SWAPPER_HPP
#define PRAAS_PROCESS_CONTROLLER_SWAPPER_HPP

#include <praas/process/controller/messages.hpp>

#include <vector>
#include <string>

#include <aws/s3/S3Client.h>
#include <aws/core/Aws.h>
#include <spdlog/spdlog.h>

namespace praas::process::swapper {

  struct Swapper
  {
    virtual ~Swapper() = default;
    virtual size_t swap_out(const std::string& location, std::vector<std::tuple<std::string, message::Message>>& mailbox) = 0;
    virtual bool swap_in(const std::string& location, message::MessageStore & mailbox) = 0;

    static constexpr std::string_view FILES_DIRECTORY = "/state";
  };

  struct DiskSwapper : Swapper
  {
    std::string swap_location;

    DiskSwapper();
     ~DiskSwapper() override = default;

    bool swap_in(const std::string& location, message::MessageStore & mailbox) override;
    size_t swap_out(const std::string& location, std::vector<std::tuple<std::string, message::Message>>& msgs) override;
  };

#if defined(WITH_AWS_DEPLOYMENT)

  struct S3Swapper : Swapper
  {
    struct S3API
    {
      Aws::SDKOptions _s3_options;

      S3API();
      ~S3API();
    };

    std::optional<Aws::S3::S3Client> _s3_client;
    std::string swap_bucket;

    static constexpr int MAX_CONNECTIONS = 64;
    static constexpr std::string_view SWAPS_DIRECTORY = "swaps";

    S3Swapper(std::string swap_bucket);

    bool swap_in(const std::string& location, message::MessageStore & mailbox) override;
    size_t swap_out(const std::string& location, std::vector<std::tuple<std::string, message::Message>>& msgs) override;

    static std::optional<S3API> api;
  };
#endif

} // namespace praas::process::swapper

#endif
