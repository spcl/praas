#include <praas/process/runtime/ipc/messages.hpp>

#include <praas/common/exceptions.hpp>

#include <spdlog/fmt/fmt.h>

namespace praas::process::runtime::ipc {

  Message::MessageVariants Message::parse() const
  {
    return parse_message(data.data());
  }

  Message::MessageVariants Message::parse_message(const int8_t* data)
  {

    int16_t type_val = *reinterpret_cast<const int16_t*>(data);
    if (type_val >= static_cast<int16_t>(Type::END_FLAG) ||
        type_val < static_cast<int16_t>(Type::GENERIC_HEADER)) {
      throw common::InvalidMessage{fmt::format("Invalid type value for Message: {}", type_val)};
    }
    Type type = static_cast<Type>(type_val);

    if (type == Type::GET_REQUEST) {
      return MessageVariants{GetRequestParsed(data + HEADER_OFFSET)};
    }

    if (type == Type::PUT_REQUEST) {
      return MessageVariants{PutRequestParsed(data + HEADER_OFFSET)};
    }

    if (type == Type::INVOCATION_REQUEST) {
      return MessageVariants{InvocationRequestParsed(data + HEADER_OFFSET)};
    }

    if (type == Type::INVOCATION_RESULT) {
      return MessageVariants{InvocationResultParsed(data + HEADER_OFFSET)};
    }

    if (type == Type::APPLICATION_UPDATE) {
      return MessageVariants{ApplicationUpdateParsed(data + HEADER_OFFSET)};
    }

    throw common::PraaSException{fmt::format("Unknown message with type number {}", type_val)};
  }

  Message::Type Message::type() const
  {
    int16_t type = *reinterpret_cast<const int16_t*>(data.data());

    if (type >= static_cast<int16_t>(Type::END_FLAG) ||
        type < static_cast<int16_t>(Type::GENERIC_HEADER)) {
      throw common::InvalidMessage{fmt::format("Invalid type value for Message: {}", type)};
    }

    return static_cast<Type>(type);
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

  int32_t GenericRequestParsed::data_len() const
  {
    // NOLINTNEXTLINE
    return *reinterpret_cast<const int32_t*>(buf);
  }

  bool GenericRequestParsed::state() const
  {
    return *reinterpret_cast<const bool*>(buf + Message::NAME_LENGTH*2 + 4);
  }

  std::string_view GenericRequestParsed::process_id() const
  {
    return std::string_view{// NOLINTNEXTLINE
                            reinterpret_cast<const char*>(buf + 4), id_len};
  }

  std::string_view GenericRequestParsed::name() const
  {
    return std::string_view{// NOLINTNEXTLINE
                            reinterpret_cast<const char*>(buf + Message::NAME_LENGTH + 4),
                            name_len};
  }

  void GenericRequest::data_len(int32_t len)
  {
    // NOLINTNEXTLINE
    *reinterpret_cast<int32_t*>(data.data() + HEADER_OFFSET) = len;
  }

  void GenericRequest::state(bool val)
  {
    *reinterpret_cast<bool*>(data.data() + HEADER_OFFSET + Message::NAME_LENGTH*2 + 4) = val;
  }

  void GenericRequest::process_id(std::string_view process_id)
  {
    if (process_id.length() > Message::NAME_LENGTH) {
      throw common::InvalidArgument{
          fmt::format("Process name too long: {} > {}", process_id.length(), Message::NAME_LENGTH)};
    }

    std::strncpy(
        // NOLINTNEXTLINE
        reinterpret_cast<char*>(data.data() + HEADER_OFFSET + 4), process_id.data(),
        Message::NAME_LENGTH
    );
    id_len = process_id.length();
  }

  void GenericRequest::name(std::string_view name)
  {
    if (name.length() > Message::NAME_LENGTH) {
      throw common::InvalidArgument{
          fmt::format("Process name too long: {} > {}", name.length(), Message::NAME_LENGTH)};
    }

    std::strncpy(
        // NOLINTNEXTLINE
        reinterpret_cast<char*>(data.data() + HEADER_OFFSET + 4 + Message::NAME_LENGTH),
        name.data(), Message::NAME_LENGTH
    );
    name_len = name.length();
  }

  int32_t InvocationRequestParsed::buffers() const
  {
    // NOLINTNEXTLINE
    return *reinterpret_cast<const int32_t*>(buf + 2*Message::ID_LENGTH + Message::NAME_LENGTH);
  }

  const int32_t* InvocationRequestParsed::buffers_lengths() const
  {
    // NOLINTNEXTLINE
    return reinterpret_cast<const int32_t*>(
        buf + 2*Message::ID_LENGTH + Message::NAME_LENGTH + sizeof(int32_t)
    );
  }

  std::string_view InvocationRequestParsed::process_id() const
  {
    return std::string_view{// NOLINTNEXTLINE
                            reinterpret_cast<const char*>(buf + Message::ID_LENGTH + Message::NAME_LENGTH), process_id_len};
  }

  std::string_view InvocationRequestParsed::invocation_id() const
  {
    return std::string_view{// NOLINTNEXTLINE
                            reinterpret_cast<const char*>(buf), id_len};
  }

  std::string_view InvocationRequestParsed::function_name() const
  {
    return std::string_view{// NOLINTNEXTLINE
                            reinterpret_cast<const char*>(buf + Message::ID_LENGTH), name_len};
  }

  void InvocationRequest::process_id(std::string_view id)
  {
    if (id.length() > Message::ID_LENGTH) {
      throw common::InvalidArgument{
          fmt::format("Invocation ID too long: {} > {}", id.length(), Message::ID_LENGTH)};
    }

    std::strncpy(
        // NOLINTNEXTLINE
        reinterpret_cast<char*>(data.data() + HEADER_OFFSET + Message::ID_LENGTH + Message::NAME_LENGTH),
        id.data(), Message::ID_LENGTH
    );
    process_id_len = id.length();
  }

  void InvocationRequest::invocation_id(std::string_view id)
  {
    if (id.length() > Message::ID_LENGTH) {
      throw common::InvalidArgument{
          fmt::format("Process ID too long: {} > {}", id.length(), Message::ID_LENGTH)};
    }

    std::strncpy(
        // NOLINTNEXTLINE
        reinterpret_cast<char*>(data.data() + HEADER_OFFSET), id.data(), Message::ID_LENGTH
    );
    id_len = id.length();
  }

  void InvocationRequest::function_name(std::string_view name)
  {
    if (name.length() > Message::NAME_LENGTH) {
      throw common::InvalidArgument{
          fmt::format("Function name too long: {} > {}", name.length(), Message::NAME_LENGTH)};
    }

    std::strncpy(
        // NOLINTNEXTLINE
        reinterpret_cast<char*>(data.data() + HEADER_OFFSET + Message::ID_LENGTH), name.data(),
        Message::NAME_LENGTH
    );
    name_len = name.length();
  }

  void InvocationRequest::buffers(int32_t buffer_len)
  {
    // NOLINTNEXTLINE
    auto ptr = reinterpret_cast<int32_t*>(
        data.data() + HEADER_OFFSET + Message::NAME_LENGTH + 2*Message::ID_LENGTH
    );
    *ptr++ = 1;
    *ptr = buffer_len;
  }

  void InvocationRequest::buffers(int32_t* begin, int32_t* end)
  {
    int elems = std::distance(begin, end);
    if (elems > InvocationRequest::MAX_BUFFERS) {
      throw common::InvalidArgument{
          fmt::format("Number of buffers too large: {} > {}", elems, InvocationRequest::MAX_BUFFERS)};
    }

    // NOLINTNEXTLINE
    auto ptr = reinterpret_cast<int32_t*>(
        data.data() + HEADER_OFFSET + Message::NAME_LENGTH + 2*Message::ID_LENGTH
    );
    *ptr++ = elems;

    while (begin != end) {
      *ptr++ = *begin++;
    }
  }

  std::string_view InvocationResultParsed::invocation_id() const
  {
    return std::string_view{// NOLINTNEXTLINE
                            reinterpret_cast<const char*>(buf), id_len};
  }

  int32_t InvocationResultParsed::buffer_length() const
  {
    // NOLINTNEXTLINE
    return *reinterpret_cast<const int32_t*>(buf + Message::ID_LENGTH);
  }

  int32_t InvocationResultParsed::return_code() const
  {
    // NOLINTNEXTLINE
    return *reinterpret_cast<const int32_t*>(buf + Message::ID_LENGTH + sizeof(int32_t));
  }

  void InvocationResult::invocation_id(std::string_view id)
  {
    if (id.length() > Message::ID_LENGTH) {
      throw common::InvalidArgument{
          fmt::format("Invocation ID too long: {} > {}", id.length(), Message::ID_LENGTH)};
    }

    std::strncpy(
        // NOLINTNEXTLINE
        reinterpret_cast<char*>(data.data() + HEADER_OFFSET), id.data(), Message::ID_LENGTH
    );
    id_len = id.length();
  }

  void InvocationResult::return_code(int32_t code)
  {
    // NOLINTNEXTLINE
    *reinterpret_cast<int32_t*>(data.data() + HEADER_OFFSET + Message::ID_LENGTH + sizeof(int32_t)) = code;
  }

  void InvocationResult::buffer_length(int32_t len)
  {
    // NOLINTNEXTLINE
    *reinterpret_cast<int32_t*>(data.data() + HEADER_OFFSET + Message::ID_LENGTH) = len;
  }

  int32_t ApplicationUpdateParsed::status_change() const
  {
    // NOLINTNEXTLINE
    return *reinterpret_cast<const int32_t*>(buf + Message::NAME_LENGTH);
  }

  std::string_view ApplicationUpdateParsed::process_id() const
  {
    return std::string_view{// NOLINTNEXTLINE
                            reinterpret_cast<const char*>(buf), id_len};
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

  void ApplicationUpdate::status_change(int32_t code)
  {
    // NOLINTNEXTLINE
    *reinterpret_cast<int32_t*>(data.data() + HEADER_OFFSET + Message::NAME_LENGTH) = code;
  }

} // namespace praas::process::runtime::ipc
