#include <praas/common/messages.hpp>

#include <praas/common/exceptions.hpp>

#include <cstring>

#include <fmt/format.h>

#include <iostream>

namespace praas::common::message {

  Message::Type Message::type() const
  {
    int16_t type = *reinterpret_cast<const int16_t*>(data.data());

    if (type >= static_cast<int16_t>(Type::END_FLAG) ||
        type < static_cast<int16_t>(Type::GENERIC_HEADER)) {
      throw common::InvalidMessage{fmt::format("Invalid type value for Message: {}", type)};
    }

    return static_cast<Type>(type);
  }

  Message::MessageVariants Message::parse()
  {
    return parse_message(data.data());
  }

  Message::MessageVariants Message::parse_message(const char* data)
  {
    return parse_message(reinterpret_cast<const int8_t*>(data));
  }

  Message::MessageVariants Message::parse_message(const int8_t* data)
  {

    int16_t type_val = *reinterpret_cast<const int16_t*>(data);
    if (type_val >= static_cast<int16_t>(Type::END_FLAG) ||
        type_val < static_cast<int16_t>(Type::GENERIC_HEADER)) {
      throw common::InvalidMessage{fmt::format("Invalid type value for Message: {}", type_val)};
    }
    Type type = static_cast<Type>(type_val);

    if (type == Type::PROCESS_CONNECTION) {
      return MessageVariants{ProcessConnectionParsed(data + HEADER_OFFSET)};
    }

    if (type == Type::SWAP_CONFIRMATION) {
      return MessageVariants{SwapConfirmationParsed(data + HEADER_OFFSET)};
    }

    if (type == Type::INVOCATION_REQUEST) {
      return MessageVariants{InvocationRequestParsed(data + HEADER_OFFSET)};
    }

    if (type == Type::INVOCATION_RESULT) {
      return MessageVariants{InvocationResultParsed(data + HEADER_OFFSET)};
    }

    if (type == Type::INVOCATION_RESULT) {
      return MessageVariants{InvocationResultParsed(data + HEADER_OFFSET)};
    }

    if (type == Type::DATAPLANE_METRICS) {
      return MessageVariants{DataPlaneMetricsParsed(data + HEADER_OFFSET)};
    }

    if (type == Type::PROCESS_CLOSURE) {
      return MessageVariants{ProcessClosureParsed(data + HEADER_OFFSET)};
    }

    throw common::NotImplementedError{};
  }

  std::string_view ProcessConnectionParsed::process_name() const
  {
    // NOLINTNEXTLINE
    return std::string_view{reinterpret_cast<const char*>(buf), process_name_len};
  }

  void ProcessConnection::process_name(const std::string& name)
  {
    if (name.length() > Message::NAME_LENGTH) {
      throw common::InvalidArgument{
          fmt::format("Process name too long: {} > {}", name.length(), Message::NAME_LENGTH)};
    }
    std::strncpy(
        // NOLINTNEXTLINE
        reinterpret_cast<char*>(data.data() + HEADER_OFFSET), name.data(), Message::NAME_LENGTH
    );
    process_name_len = name.length();
  }

  Message::Type ProcessConnectionParsed::type()
  {
    return Message::Type::PROCESS_CONNECTION;
  }

  Message::Type SwapConfirmationParsed::type()
  {
    return Message::Type::SWAP_CONFIRMATION;
  }

  std::string_view InvocationRequestParsed::invocation_id() const
  {
    return std::string_view{
    // NOLINTNEXTLINE
        reinterpret_cast<const char*>(buf + Message::NAME_LENGTH + 4), invocation_id_len};
  }

  std::string_view InvocationRequestParsed::function_name() const
  {
    // NOLINTNEXTLINE
    return std::string_view{reinterpret_cast<const char*>(buf + 4), fname_len};
  }

  int32_t InvocationRequestParsed::payload_size() const
  {
    // NOLINTNEXTLINE
    return *reinterpret_cast<const int32_t*>(buf);
  }

  Message::Type InvocationRequestParsed::type()
  {
    return Message::Type::INVOCATION_REQUEST;
  }

  void InvocationRequest::invocation_id(const std::string& name)
  {
    if (name.length() > Message::ID_LENGTH) {
      throw common::InvalidArgument{
          fmt::format("Invocation ID too long: {} > {}", name.length(), Message::ID_LENGTH)};
    }
    std::strncpy(
        // NOLINTNEXTLINE
        reinterpret_cast<char*>(data.data() + HEADER_OFFSET + Message::NAME_LENGTH + 4),
        name.data(), Message::ID_LENGTH
    );
    invocation_id_len = name.length();
  }

  void InvocationRequest::function_name(const std::string& name)
  {
    if (name.length() > Message::NAME_LENGTH) {
      throw common::InvalidArgument{
          fmt::format("Function name too long: {} > {}", name.length(), Message::NAME_LENGTH)};
    }
    std::strncpy(
        // NOLINTNEXTLINE
        reinterpret_cast<char*>(data.data() + HEADER_OFFSET + 4), name.data(), Message::NAME_LENGTH
    );
    fname_len = name.length();
  }

  void InvocationRequest::payload_size(int32_t size)
  {
    if (size < 0) {
      throw common::InvalidArgument{fmt::format("Payload size too small: {}", size)};
    }

    // NOLINTNEXTLINE
    *reinterpret_cast<int32_t*>(data.data() + HEADER_OFFSET) = size;
  }

  std::string_view InvocationResultParsed::invocation_id() const
  {
    // NOLINTNEXTLINE
    return std::string_view{reinterpret_cast<const char*>(buf), invocation_id_len};
  }

  int32_t InvocationResultParsed::response_size() const
  {
    // NOLINTNEXTLINE
    return *reinterpret_cast<const int32_t*>(buf + Message::NAME_LENGTH);
  }

  Message::Type InvocationResultParsed::type()
  {
    return Message::Type::INVOCATION_RESULT;
  }

  void InvocationResult::invocation_id(const std::string& invocation_id)
  {
    if (invocation_id.length() > Message::NAME_LENGTH) {
      throw common::InvalidArgument{fmt::format(
          "Invocation ID too long: {} > {}", invocation_id.length(), Message::NAME_LENGTH
      )};
    }
    std::strncpy(
        // NOLINTNEXTLINE
        reinterpret_cast<char*>(data.data() + HEADER_OFFSET), invocation_id.data(),
        Message::NAME_LENGTH
    );
    invocation_id_len = invocation_id.length();
  }

  void InvocationResult::response_size(int32_t size)
  {
    if (size < 0) {
      throw common::InvalidArgument{fmt::format("Response size too small: {}", size)};
    }

    // NOLINTNEXTLINE
    *reinterpret_cast<int32_t*>(data.data() + HEADER_OFFSET + Message::NAME_LENGTH) = size;
  }

  int32_t DataPlaneMetricsParsed::invocations() const
  {
    // NOLINTNEXTLINE
    return *reinterpret_cast<const int32_t*>(buf);
  }

  int32_t DataPlaneMetricsParsed::computation_time() const
  {
    // NOLINTNEXTLINE
    return *reinterpret_cast<const int32_t*>(buf + 4);
  }

  uint64_t DataPlaneMetricsParsed::last_invocation_timestamp() const
  {
    // NOLINTNEXTLINE
    return *reinterpret_cast<const uint64_t*>(buf + 8);
  }

  void DataPlaneMetrics::invocations(int32_t invocations)
  {
    if (invocations < 0) {
      throw common::InvalidArgument{fmt::format("Incorrect number of invocations {}", invocations)};
    }

    // NOLINTNEXTLINE
    *reinterpret_cast<int32_t*>(data.data() + HEADER_OFFSET) = invocations;
  }

  void DataPlaneMetrics::computation_time(int32_t time)
  {
    if (time < 0) {
      throw common::InvalidArgument{fmt::format("Incorrect computation time {}", time)};
    }

    // NOLINTNEXTLINE
    *reinterpret_cast<int32_t*>(data.data() + HEADER_OFFSET + 4) = time;
  }

  void DataPlaneMetrics::last_invocation_timestamp(uint64_t timestamp)
  {
    // NOLINTNEXTLINE
    *reinterpret_cast<uint64_t*>(data.data() + HEADER_OFFSET + 8) = timestamp;
  }

  Message::Type DataPlaneMetricsParsed::type()
  {
    return Message::Type::DATAPLANE_METRICS;
  }

  Message::Type ProcessClosureParsed::type()
  {
    return Message::Type::PROCESS_CLOSURE;
  }

} // namespace praas::common::message
