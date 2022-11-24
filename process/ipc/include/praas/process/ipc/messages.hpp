#ifndef PRAAS_IPC_MESSAGES_HPP
#define PRAAS_IPC_MESSAGES_HPP

#include <cstring>
#include <memory>
#include <string>
#include <variant>

namespace praas::process::ipc {

  struct GetRequestParsed;
  struct PutRequestParsed;
  struct InvocationRequestParsed;
  struct InvocationResultParsed;

  struct Message {

    enum class Type : int16_t {
      GENERIC_HEADER = 0,
      GET_REQUEST = 1,
      PUT_REQUEST,
      INVOCATION_REQUEST,
      INVOCATION_RESULT,
      END_FLAG
    };

    static constexpr uint16_t HEADER_OFFSET = 6;
    static constexpr uint16_t NAME_LENGTH = 32;
    static constexpr uint16_t BUF_SIZE = 128;

    std::array<int8_t, BUF_SIZE> data{};

    Message(Type type = Type::GENERIC_HEADER)
    {
      // NOLINTNEXTLINE
      *reinterpret_cast<int16_t*>(data.data()) = static_cast<int16_t>(type);
    }

    Type type() const;
    size_t total_length() const;

    const int8_t* bytes() const
    {
      return data.data();
    }

    void total_length(size_t);

    using MessageVariants = std::variant<
        GetRequestParsed, PutRequestParsed, InvocationRequestParsed, InvocationResultParsed>;

    MessageVariants parse();

    static MessageVariants parse_message(const int8_t* data);
  };

  struct GenericRequestParsed {
    const int8_t* buf;
    size_t id_len;
    size_t name_len;

    GenericRequestParsed(const int8_t* buf)
        : buf(buf),
          // NOLINTNEXTLINE
          id_len(strnlen(reinterpret_cast<const char*>(buf + 4), Message::NAME_LENGTH)),
          name_len(strnlen(
            // NOLINTNEXTLINE
            reinterpret_cast<const char*>(buf + 4 + Message::NAME_LENGTH), Message::NAME_LENGTH
          ))
    {
    }

    int32_t data_len() const;
    std::string_view process_id() const;
    std::string_view name() const;
  };

  struct GenericRequest : Message, GenericRequestParsed {

    GenericRequest(Type msg_type)
        : Message(msg_type), GenericRequestParsed(this->data.data() + HEADER_OFFSET)
    {
    }

    void data_len(int32_t);
    void process_id(const std::string&);
    void name(const std::string&);
  };

  struct GetRequestParsed : GenericRequestParsed {

    GetRequestParsed(const int8_t* buf) : GenericRequestParsed(buf) {}
  };

  struct GetRequest : GenericRequest {

    GetRequest() : GenericRequest(Type::GET_REQUEST) {}

  };

  struct PutRequestParsed : GenericRequestParsed {

    PutRequestParsed(const int8_t* buf) : GenericRequestParsed(buf) {}
  };

  struct PutRequest : GenericRequest {

    PutRequest() : GenericRequest(Type::PUT_REQUEST) {}

  };

  struct InvocationRequestParsed {
    const int8_t* buf;

    std::string_view invocation_id() const;
    std::string_view function_name() const;
    int32_t buffers() const;
    int32_t* buffers_lengths() const;
  };

  struct InvocationRequest {};

  struct InvocationResultParsed {
    const int8_t* buf;

    std::string_view invocation_id() const;
    int32_t buffer_length() const;
  };

  struct InvocationResult {};

} // namespace praas::process::ipc

#endif
