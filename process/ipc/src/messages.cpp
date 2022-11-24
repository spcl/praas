#include <praas/process/ipc/messages.hpp>

#include <praas/common/exceptions.hpp>

#include <fmt/format.h>

namespace praas::process::ipc {

  Message::MessageVariants Message::parse()
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

    //if (type == Type::INVOCATION_REQUEST) {
    //  return MessageVariants{InvocationRequestParsed(data + HEADER_OFFSET)};
    //}

    //if (type == Type::INVOCATION_RESULT) {
    //  return MessageVariants{InvocationResultParsed(data + HEADER_OFFSET)};
    //}

    throw common::NotImplementedError{};
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

  size_t Message::total_length() const
  {
    // NOLINTNEXTLINE
    return *reinterpret_cast<const size_t*>(data.data() + 2);
  }

  void Message::total_length(size_t len)
  {
    // NOLINTNEXTLINE
    *reinterpret_cast<size_t*>(data.data() + 2) = len;
  }

  int32_t GenericRequestParsed::data_len() const
  {
    // NOLINTNEXTLINE
    return *reinterpret_cast<const int32_t*>(buf);
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

  void GenericRequest::process_id(const std::string& process_id)
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

  void GenericRequest::name(const std::string& name)
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

} // namespace praas::process::ipc
