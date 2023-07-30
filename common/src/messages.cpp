#include <praas/common/messages.hpp>

#include <praas/common/exceptions.hpp>

#include <cstring>

// #include <fmt/format.h>
#include <spdlog/fmt/fmt.h>

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

  int32_t Message::total_length() const
  {
    // NOLINTNEXTLINE
    return *reinterpret_cast<const int32_t*>(data.data() + 2);
  }

  void Message::total_length(int32_t len)
  {
    // NOLINTNEXTLINE
    *reinterpret_cast<int32_t*>(data.data() + 2) = len;
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

    if (type == Type::SWAP_REQUEST) {
      return MessageVariants{SwapRequestParsed(data + HEADER_OFFSET)};
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

    if (type == Type::DATAPLANE_METRICS) {
      return MessageVariants{DataPlaneMetricsParsed(data + HEADER_OFFSET)};
    }

    if (type == Type::PROCESS_CLOSURE) {
      return MessageVariants{ProcessClosureParsed(data + HEADER_OFFSET)};
    }

    if (type == Type::APPLICATION_UPDATE) {
      return MessageVariants{ApplicationUpdateParsed(data + HEADER_OFFSET)};
    }

    if (type == Type::PUT_MESSAGE) {
      return MessageVariants{PutMessageParsed(data + HEADER_OFFSET)};
    }

    throw common::NotImplementedError{};
  }

  std::string_view ProcessConnectionParsed::process_name() const
  {
    // NOLINTNEXTLINE
    return std::string_view{reinterpret_cast<const char*>(buf), process_name_len};
  }

  void ProcessConnection::process_name(std::string_view name)
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

  Message::Type SwapRequestParsed::type()
  {
    return Message::Type::SWAP_REQUEST;
  }

  std::string_view SwapRequestParsed::path() const
  {
    // NOLINTNEXTLINE
    return std::string_view{reinterpret_cast<const char*>(buf), path_len};
  }

  void SwapRequest::path(const std::string& path)
  {
    this->path(std::string_view{path});
  }

  void SwapRequest::path(std::string_view path)
  {
    if (path.length() > Message::ID_LENGTH) {
      throw common::InvalidArgument{
          fmt::format("Swap location ID too long: {} > {}", path.length(), Message::ID_LENGTH)};
    }
    std::strncpy(
        // NOLINTNEXTLINE
        reinterpret_cast<char*>(data.data() + HEADER_OFFSET), path.data(), Message::ID_LENGTH
    );
    path_len = path.length();
  }

  Message::Type SwapConfirmationParsed::type()
  {
    return Message::Type::SWAP_CONFIRMATION;
  }

  int32_t SwapConfirmationParsed::swap_size() const
  {
    // NOLINTNEXTLINE
    return *reinterpret_cast<const int32_t*>(buf);
  }

  void SwapConfirmation::swap_size(int32_t size)
  {
    // NOLINTNEXTLINE
    *reinterpret_cast<int32_t*>(data.data() + HEADER_OFFSET) = size;
  }

  std::string_view InvocationRequestParsed::invocation_id() const
  {
    return std::string_view{// NOLINTNEXTLINE
                            reinterpret_cast<const char*>(buf + Message::NAME_LENGTH + 4),
                            invocation_id_len};
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

  void InvocationRequest::invocation_id(std::string_view name)
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

  void InvocationRequest::function_name(std::string_view name)
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

  int32_t InvocationResultParsed::return_code() const
  {
    // NOLINTNEXTLINE
    return *reinterpret_cast<const int32_t*>(buf + Message::NAME_LENGTH);
  }

  Message::Type InvocationResultParsed::type()
  {
    return Message::Type::INVOCATION_RESULT;
  }

  void InvocationResult::invocation_id(std::string_view invocation_id)
  {
    if (invocation_id.length() > Message::ID_LENGTH) {
      throw common::InvalidArgument{fmt::format(
          "Invocation ID too long: {} > {}", invocation_id.length(), Message::ID_LENGTH
      )};
    }
    std::strncpy(
        // NOLINTNEXTLINE
        reinterpret_cast<char*>(data.data() + HEADER_OFFSET), invocation_id.data(),
        Message::ID_LENGTH
    );
    invocation_id_len = invocation_id.length();
  }

  void InvocationResult::return_code(int32_t size)
  {
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

  int32_t ApplicationUpdateParsed::status_change() const
  {
    // NOLINTNEXTLINE
    return *reinterpret_cast<const int32_t*>(buf + Message::NAME_LENGTH + Message::ID_LENGTH);
  }

  int32_t ApplicationUpdateParsed::port() const
  {
    // NOLINTNEXTLINE
    return *reinterpret_cast<const int32_t*>(
        buf + Message::NAME_LENGTH + Message::ID_LENGTH + sizeof(int32_t)
    );
  }

  std::string_view ApplicationUpdateParsed::process_id() const
  {
    return std::string_view{// NOLINTNEXTLINE
                            reinterpret_cast<const char*>(buf), id_len};
  }

  std::string_view ApplicationUpdateParsed::ip_address() const
  {
    return std::string_view{// NOLINTNEXTLINE
                            reinterpret_cast<const char*>(buf + Message::NAME_LENGTH), ip_len};
  }

  void ApplicationUpdate::process_id(std::string_view id)
  {
    if (id.length() > Message::NAME_LENGTH) {
      throw common::InvalidArgument{
          fmt::format("Process ID too long: {} > {}", id.length(), Message::NAME_LENGTH)};
    }

    std::strncpy(
        // NOLINTNEXTLINE
        reinterpret_cast<char*>(data.data() + HEADER_OFFSET), id.data(), Message::NAME_LENGTH
    );
    id_len = id.length();
  }

  void ApplicationUpdate::ip_address(std::string_view id)
  {
    if (id.length() > Message::NAME_LENGTH) {
      throw common::InvalidArgument{
          fmt::format("Process ID too long: {} > {}", id.length(), Message::NAME_LENGTH)};
    }

    std::strncpy(
        // NOLINTNEXTLINE
        reinterpret_cast<char*>(data.data() + HEADER_OFFSET + Message::NAME_LENGTH), id.data(),
        Message::ID_LENGTH
    );
    ip_len = id.length();
  }

  void ApplicationUpdate::status_change(int32_t code)
  {
    // NOLINTNEXTLINE
    *reinterpret_cast<int32_t*>(
        data.data() + HEADER_OFFSET + Message::NAME_LENGTH + Message::ID_LENGTH
    ) = code;
  }

  void ApplicationUpdate::port(int32_t port)
  {
    // NOLINTNEXTLINE
    *reinterpret_cast<int32_t*>(
        data.data() + HEADER_OFFSET + Message::NAME_LENGTH + Message::ID_LENGTH + sizeof(int32_t)
    ) = port;
  }

  std::string_view PutMessageParsed::name() const
  {
    return std::string_view{// NOLINTNEXTLINE
                            reinterpret_cast<const char*>(buf), name_len};
  }

  std::string_view PutMessageParsed::process_id() const
  {
    return std::string_view{// NOLINTNEXTLINE
                            reinterpret_cast<const char*>(buf + Message::NAME_LENGTH), id_len};
  }

  // int32_t PutMessageParsed::payload_size() const
  //{
  //   // NOLINTNEXTLINE
  //   return *reinterpret_cast<const int32_t*>(buf + 2*Message::NAME_LENGTH);
  // }

  void PutMessage::name(std::string_view name)
  {
    if (name.length() > Message::NAME_LENGTH) {
      throw common::InvalidArgument{
          fmt::format("Message name too long: {} > {}", name.length(), Message::NAME_LENGTH)};
    }

    std::strncpy(
        // NOLINTNEXTLINE
        reinterpret_cast<char*>(data.data() + HEADER_OFFSET), name.data(), Message::NAME_LENGTH
    );
    name_len = name.length();
  }

  void PutMessage::process_id(std::string_view name)
  {
    if (name.length() > Message::NAME_LENGTH) {
      throw common::InvalidArgument{
          fmt::format("Message name too long: {} > {}", name.length(), Message::NAME_LENGTH)};
    }

    std::strncpy(
        // NOLINTNEXTLINE
        reinterpret_cast<char*>(data.data() + HEADER_OFFSET + Message::NAME_LENGTH), name.data(),
        Message::NAME_LENGTH
    );
    id_len = name.length();
  }

  int32_t PutMessageParsed::total_length() const
  {
    // NOLINTNEXTLINE
    // FIXME: this is an ugly hack
    return *reinterpret_cast<const int32_t*>(buf - 4);
  }

  int32_t InvocationResultParsed::total_length() const
  {
    // NOLINTNEXTLINE
    // FIXME: this is an ugly hack
    return *reinterpret_cast<const int32_t*>(buf - 4);
  }

  int32_t InvocationRequestParsed::total_length() const
  {
    // NOLINTNEXTLINE
    // FIXME: this is an ugly hack
    return *reinterpret_cast<const int32_t*>(buf - 4);
  }

  // void PutMessage::payload_size(int32_t size)
  //{
  //   // NOLINTNEXTLINE
  //   *reinterpret_cast<int32_t*>(data.data() + HEADER_OFFSET + 2*Message::NAME_LENGTH) = size;
  // }

} // namespace praas::common::message
