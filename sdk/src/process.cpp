#include <praas/sdk/process.hpp>

#include <praas/common/messages.hpp>
#include <variant>

namespace praas::sdk {

  Process::Process(const std::string& addr, int port):
    _addr(addr, port)
  {}

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
    if(!status)
      return false;

    praas::common::message::ProcessConnection req;
    req.process_name("DATAPLANE");
    _dataplane.write_n(req.bytes(), req.BUF_SIZE);

    // Now wait for the confirmation;
    praas::common::message::Message response;
    _dataplane.read_n(response.data.data(), response.BUF_SIZE);

    auto parsed_msg = response.parse();
    if(!std::holds_alternative<praas::common::message::ProcessConnectionParsed>(parsed_msg)) {
      return false;
    }
    auto& result = std::get<common::message::ProcessConnectionParsed>(parsed_msg);

    return result.process_name() == "CORRECT";
  }

  InvocationResult Process::invoke(std::string_view function_name, std::string invocation_id, char* ptr, size_t len)
  {
    praas::common::message::InvocationRequest msg;
    msg.function_name(function_name);
    msg.invocation_id(invocation_id);
    msg.payload_size(len);

    _dataplane.write_n(msg.bytes(), msg.BUF_SIZE);
    _dataplane.write_n(ptr, len);

    // FIXME: fix message parsing
    praas::common::message::Message response;
    _dataplane.read_n(response.data.data(), response.BUF_SIZE);

    auto parsed_msg = response.parse();
    if(!std::holds_alternative<common::message::InvocationResultParsed>(parsed_msg)) {
      return {1, nullptr, 0};
    }

    auto& result = std::get<common::message::InvocationResultParsed>(parsed_msg);

    size_t payload_bytes = response.total_length();
    std::unique_ptr<char[]> payload{};
    if(payload_bytes > 0) {
      payload.reset(new char[payload_bytes]);
      _dataplane.read_n(payload.get(), payload_bytes);
    }

    return {result.return_code(), std::move(payload), payload_bytes};
  }

};
