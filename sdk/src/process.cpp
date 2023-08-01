#include <praas/sdk/process.hpp>

#include <praas/common/messages.hpp>
#include <praas/common/sockets.hpp>

#include <variant>
#include "praas/common/exceptions.hpp"

namespace praas::sdk {

  Process::Process(const std::string& addr, int port, bool disable_nagle)
      : _disable_nagle(disable_nagle), _addr(addr, port)
  {
  }

  Process::~Process()
  {
    _dataplane.close();
  }

  void Process::disconnect()
  {
    _dataplane.close();
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
    req.process_name("DATAPLANE");
    _dataplane.write_n(req.bytes(), req.BUF_SIZE);

    // Now wait for the confirmation;
    praas::common::message::MessageData response;
    _dataplane.read_n(response.data(), praas::common::message::MessageConfig::BUF_SIZE);

    auto parsed_msg = praas::common::message::MessageParser::parse(response);
    if (!std::holds_alternative<praas::common::message::ProcessConnectionPtr>(parsed_msg)) {
      return false;
    }
    auto& result = std::get<common::message::ProcessConnectionPtr>(parsed_msg);

    return result.process_name() == "CORRECT";
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

    _dataplane.write_n(msg.bytes(), msg.BUF_SIZE);
    if (len > 0) {
      _dataplane.write_n(ptr, len);
    }

    praas::common::message::MessageData response;
    _dataplane.read_n(response.data(), praas::common::message::MessageConfig::BUF_SIZE);

    auto parsed_msg = praas::common::message::MessageParser::parse(response);
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
