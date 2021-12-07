
#include <charconv>

#include <sockpp/tcp_connector.h>
#include <spdlog/spdlog.h>

#include "backend.hpp"
#include "server.hpp"

namespace praas::control_plane::backend {

  std::tuple<std::string_view, std::string_view> split(std::string & str, char split_character)
  {
    ssize_t pos = str.find(split_character);
    if(pos == std::string::npos)
      return std::make_tuple("", "");
    else
      return std::make_tuple(
        std::string_view(str.data(), pos),
        std::string_view(str.data() + pos + 1, str.length() - pos - 1)
      );
  }

  Backend* Backend::construct(Options & options)
  {
    Backend* ptr = nullptr;
    if(options.backend == FunctionBackendType::LOCAL) {
      ptr = LocalBackend::create(options.local_server);
      if(!ptr)
        spdlog::error("Couldn't connect to a local process server at {}", options.local_server);
    } else {
      // FIXME
    }
    if(ptr) {
      ptr->controller_ip_address = options.ip_address;
      ptr->controller_port = options.port;
    }
    return ptr;
  }

  LocalBackend::LocalBackend(sockpp::tcp_connector && conn)
  {
    connection = std::move(conn);
  }

  LocalBackend* LocalBackend::create(std::string local_server_addr)
  {
    auto [ip_address, port_str] = split(local_server_addr, ':');
    int port;
    // https://stackoverflow.com/questions/56634507/safely-convert-stdstring-view-to-int-like-stoi-or-atoi
    std::from_chars(port_str.data(), port_str.data() + port_str.size(), port);
    spdlog::debug("Connect control plane to local worker at {}:{}.", ip_address, port);
    try {
      sockpp::tcp_connector connection;
      if(connection.connect(sockpp::inet_address(std::string{ip_address}, port)))
        return new LocalBackend(std::move(connection));
      else
        return nullptr;
    } catch (...) {
      return nullptr;
    }
  }

  void LocalBackend::allocate_process(
    std::string process_name, std::string process_id, int16_t max_sessions
  )
  {
    ssize_t size = req.fill(max_sessions, controller_port, controller_ip_address, process_id);
    // FIXME: handle errors here
    connection.write(req.data, size);
    spdlog::debug(
      "Allocating a new process {} with max sessions {}, process will talk to {}:{}",
      process_id, max_sessions, controller_ip_address, controller_port
    );
  }

  void AWSBackend::allocate_process(
    std::string process_name, std::string process_id, int16_t max_sessions
  )
  {
    // FIXME
  }

}

