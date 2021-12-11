
#ifndef __CONTROLL_PLANE_BACKEND_HPP__
#define __CONTROLL_PLANE_BACKEND_HPP__

#include <string>
#include <optional>

#include <sockpp/tcp_connector.h>

#include "../common/messages.hpp"

namespace praas::control_plane {

  struct Options;

  namespace backend {

    struct Backend {
      std::string controller_ip_address;
      int32_t controller_port;

      virtual ~Backend() {}

      virtual void allocate_process(
        std::string process_name, std::string process_id, int16_t max_sessions
      ) = 0;
      static Backend* construct(Options &);
    };

    struct LocalBackend: Backend {
      sockpp::tcp_connector connection;
      praas::common::ProcessRequest req;

      using Backend::controller_ip_address;
      using Backend::controller_port;

      LocalBackend(sockpp::tcp_connector &&);
      void allocate_process(
        std::string process_name, std::string process_id, int16_t max_sessions
      ) override;
      static LocalBackend* create(std::string local_server_addr);
    };

    struct AWSBackend: Backend {
      void allocate_process(
        std::string process_name, std::string process_id, int16_t max_sessions
      ) override;
    };

  }
}

#endif

