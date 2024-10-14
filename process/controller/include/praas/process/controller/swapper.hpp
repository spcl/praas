#ifndef PRAAS_PROCESS_CONTROLLER_SWAPPER_HPP
#define PRAAS_PROCESS_CONTROLLER_SWAPPER_HPP

#include <praas/process/controller/messages.hpp>

#include <vector>
#include <string>

#include <spdlog/spdlog.h>

namespace praas::process::swapper {

  struct Swapper
  {
    virtual ~Swapper() = default;
    virtual size_t swap_out(const std::string& location, std::vector<std::tuple<std::string, message::Message>>& mailbox) = 0;

    static constexpr std::string_view FILES_DIRECTORY = "/state";
  };

  struct DiskSwapper : Swapper
  {
    std::string swap_location;

    DiskSwapper();
    virtual ~DiskSwapper() = default;

    size_t swap_out(const std::string& location, std::vector<std::tuple<std::string, message::Message>>& msgs) override;
  };


} // namespace praas::process::swapper

#endif
