#include <praas/sdk/process.hpp>

#include <variant>

#include <praas/common/messages.hpp>
#include <praas/common/sockets.hpp>

#include <spdlog/spdlog.h>

namespace praas::sdk {

  Process::Process(
    std::string app, std::string pid, const std::string& addr, int port, bool disable_nagle
  )
    : app_name(std::move(app)), process_id(std::move(pid)), _disable_nagle(disable_nagle), _addr(addr, port)
  {
  }

  Process::~Process()
  {
    _dataplane.close();
  }

  void Process::disconnect()
  {
    _dataplane.close();
    _disconnected = true;
  }

  bool Process::connect()
  {
    bool status = _dataplane.connect(_addr);
    if (!status) {
      return false;
    }

    if (_disable_nagle) {
      common::sockets::disable_nagle(_dataplane.handle());
    }

    praas::common::message::ProcessConnectionData req;
    req.process_name("DATA_PLANE");
    _dataplane.write_n(req.bytes(), req.BUF_SIZE);

    // Now wait for the confirmation;
    praas::common::message::MessageData response;
    auto read_bytes =
        _dataplane.read_n(response.data(), praas::common::message::MessageConfig::BUF_SIZE);
    if (read_bytes != praas::common::message::MessageConfig::BUF_SIZE) {
      return false;
    }

    auto parsed_msg = praas::common::message::MessageParser::parse(response);
    if (!std::holds_alternative<praas::common::message::ProcessConnectionPtr>(parsed_msg)) {
      return false;
    }
    auto& result = std::get<common::message::ProcessConnectionPtr>(parsed_msg);

    _disconnected = false;

    return result.process_name() == "CORRECT";
  }

  bool Process::is_alive()
  {
    // Not sure this is fully robust but seems to work for our case
    if(_disconnected) {
      return false;
    }

    ssize_t read_bytes = recv(_dataplane.handle(), _response.data(), praas::common::message::MessageConfig::BUF_SIZE, MSG_DONTWAIT);

    if(read_bytes == -1 && errno == EAGAIN) {
      return true;
    }

    if(read_bytes == 0) {
      return false;
    }

    if(read_bytes != praas::common::message::MessageConfig::BUF_SIZE) {
      auto parsed_msg = praas::common::message::MessageParser::parse(_response);
      if (!std::holds_alternative<common::message::ProcessClosurePtr>(parsed_msg)) {
        spdlog::error("Unknown message received from the process!");
      } else {
        _disconnected = true;
      }

      return false;
    }

  }

  InvocationResult
  Process::invoke(std::string_view function_name, std::string invocation_id, char* ptr, size_t len)
  {
    if (!_dataplane.is_connected()) {
      throw common::InvalidProcessState("Not connected!");
    }
    praas::common::message::InvocationRequestData msg;
    msg.function_name(function_name);
    msg.invocation_id(invocation_id);
    msg.payload_size(len);

    auto written = _dataplane.write_n(msg.bytes(), msg.BUF_SIZE);
    if(written != msg.BUF_SIZE) {
      return {1, nullptr, 0};
    }

    if (len > 0) {
      auto written = _dataplane.write_n(ptr, len);
      if(written != len) {
        return {1, nullptr, 0};
      }
    }

    auto read_bytes = _dataplane.read_n(_response.data(), praas::common::message::MessageConfig::BUF_SIZE);
    if(read_bytes < praas::common::message::MessageConfig::BUF_SIZE) {
      return {1, nullptr, 0};
    }

    auto parsed_msg = praas::common::message::MessageParser::parse(_response);
    if (!std::holds_alternative<common::message::InvocationResultPtr>(parsed_msg)) {
      return {1, nullptr, 0};
    }

    auto& result = std::get<common::message::InvocationResultPtr>(parsed_msg);

    size_t payload_bytes = result.total_length();
    std::unique_ptr<char[]> payload{};
    if (payload_bytes > 0) {
      payload.reset(new char[payload_bytes]);
      _dataplane.read_n(payload.get(), payload_bytes);
    }

    if (result.return_code() < 0) {
      // We failed - the payload contains the error message
      return {result.return_code() * -1, nullptr, 0, std::string{payload.get(), payload_bytes}};
    }

    return {result.return_code(), std::move(payload), payload_bytes};
  }

}; // namespace praas::sdk
