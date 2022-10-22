
#include <cstring>

#include <praas/common/messages.hpp>

namespace praas::common {

  std::unique_ptr<MessageType> Header::parse()
  {
    int16_t type = *reinterpret_cast<int16_t*>(data.data());
    if (type == static_cast<int16_t>(MessageType::Type::PROCESS_CONNECTION)) {
      return std::make_unique<ProcessConnectionMessage>(data.data() + 2);
    } else if (type == static_cast<int16_t>(MessageType::Type::SWAP_CONFIRMATION)) {
      return std::make_unique<SwapConfirmation>(data.data() + 2);
    } else if (type == static_cast<int16_t>(MessageType::Type::INVOCATION_RESPONSE)) {
      return std::make_unique<InvocationResponseMessage>(data.data() + 2);
    } else if (type == static_cast<int16_t>(MessageType::Type::DATAPLANE_METRICS)) {
      return std::make_unique<DataPlaneMetricsMessage>(data.data() + 2);
    } else if (type == static_cast<int16_t>(MessageType::Type::PROCESS_CLOSURE)) {
      return std::make_unique<ClosureMessage>(data.data() + 2);
    } else {
      return nullptr;
    }
  }

  std::string ProcessConnectionMessage::process_name() const
  {
    return std::string{reinterpret_cast<char*>(buf + 2), Header::NAME_LENGTH};
  }

  MessageType::Type ProcessConnectionMessage::type() const
  {
    return MessageType::Type::PROCESS_CONNECTION;
  }

  MessageType::Type SwapConfirmation::type() const
  {
    return MessageType::Type::SWAP_CONFIRMATION;
  }

  std::string InvocationResponseMessage::invocation_id() const
  {
    return std::string{reinterpret_cast<char*>(buf), Header::NAME_LENGTH};
  }

  int32_t InvocationResponseMessage::response_size() const
  {
    return *reinterpret_cast<int32_t*>(buf + Header::NAME_LENGTH);
  }

  MessageType::Type InvocationResponseMessage::type() const
  {
    return MessageType::Type::INVOCATION_RESPONSE;
  }

  int32_t DataPlaneMetricsMessage::invocations() const
  {
    return *reinterpret_cast<int32_t*>(buf);
  }

  int32_t DataPlaneMetricsMessage::computation_time() const
  {
    return *reinterpret_cast<int32_t*>(buf + 4);
  }

  uint64_t DataPlaneMetricsMessage::last_invocation_timestamp() const
  {
    return *reinterpret_cast<uint64_t*>(buf + 8);
  }

  MessageType::Type DataPlaneMetricsMessage::type() const
  {
    return MessageType::Type::DATAPLANE_METRICS;
  }

  MessageType::Type ClosureMessage::type() const
  {
    return MessageType::Type::PROCESS_CLOSURE;
  }

  //int16_t SessionRequest::max_functions()
  //{
  //  return *reinterpret_cast<int16_t*>(data.data() + 2);
  //}

  //int32_t SessionRequest::memory_size()
  //{
  //  return *reinterpret_cast<int32_t*>(data.data() + 4);
  //}

  //std::string SessionRequest::session_id()
  //{
  //  return std::string{reinterpret_cast<char*>(data.data() + 10), 16};
  //}

  //ssize_t SessionRequest::fill(std::string session_id, int32_t max_functions, int32_t memory_size)
  //{
  //  *reinterpret_cast<int16_t*>(data.data()) =
  //      static_cast<int16_t>(Request::Type::SESSION_ALLOCATION);
  //  *reinterpret_cast<int32_t*>(data.data() + 2) = max_functions;
  //  *reinterpret_cast<int32_t*>(data.data() + 6) = memory_size;
  //  std::strncpy(reinterpret_cast<char*>(data.data() + 10), session_id.data(), 16);
  //  return EXPECTED_LENGTH;
  //}

  //int16_t ProcessRequest::max_sessions()
  //{
  //  return *reinterpret_cast<int16_t*>(data.data() + 2);
  //}

  //int32_t ProcessRequest::port()
  //{
  //  return *reinterpret_cast<int32_t*>(data.data() + 4);
  //}

  //std::string ProcessRequest::ip_address()
  //{
  //  return std::string{reinterpret_cast<char*>(data.data() + 8), 15};
  //}

  //std::string ProcessRequest::process_id()
  //{
  //  return std::string{reinterpret_cast<char*>(data.data() + 23), 16};
  //}

  //ssize_t ProcessRequest::fill(
  //    int16_t sessions, int32_t port, std::string ip_address, std::string process_id
  //)
  //{
  //  *reinterpret_cast<int16_t*>(data.data()) =
  //      static_cast<int16_t>(Request::Type::PROCESS_ALLOCATION);
  //  *reinterpret_cast<int16_t*>(data.data() + 2) = sessions;
  //  *reinterpret_cast<int32_t*>(data.data() + 4) = port;
  //  std::strncpy(reinterpret_cast<char*>(data.data() + 8), ip_address.data(), 15);
  //  std::strncpy(reinterpret_cast<char*>(data.data() + 23), process_id.data(), 16);
  //  return EXPECTED_LENGTH;
  //}

  //ssize_t
  //FunctionRequest::fill(std::string function_name, std::string function_id, int32_t payload_size)
  //{
  //  *reinterpret_cast<int16_t*>(data.data()) =
  //      static_cast<int16_t>(Request::Type::FUNCTION_INVOCATION);
  //  *reinterpret_cast<int16_t*>(data.data() + 2) = payload_size;
  //  std::strncpy(reinterpret_cast<char*>(data.data() + 6), function_name.data(), 16);
  //  std::strncpy(reinterpret_cast<char*>(data.data() + 22), function_id.data(), 16);
  //  return EXPECTED_LENGTH;
  //}

} // namespace praas::common
