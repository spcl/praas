#ifndef PRAAS_PROCESS_RUNTIME_IPC_MESSAGES_HPP
#define PRAAS_PROCESS_RUNTIME_IPC_MESSAGES_HPP

#include <praas/common/exceptions.hpp>

#include <cstring>
#include <memory>
#include <string>
#include <variant>

// #include <fmt/format.h>
#include <spdlog/fmt/fmt.h>

namespace praas::process::runtime::internal::ipc {

  template <class... Ts>
  struct overloaded : Ts... {
    using Ts::operator()...;
  };
  template <class... Ts>
  overloaded(Ts...) -> overloaded<Ts...>;

  struct GetRequestParsed;
  struct PutRequestParsed;
  struct InvocationRequestParsed;
  struct InvocationResultParsed;
  struct StateKeysRequestParsed;
  struct StateKeysResultParsed;
  struct ApplicationUpdateParsed;

  struct Message {

    enum class Type : int16_t {
      GENERIC_HEADER = 0,
      GET_REQUEST = 1,
      PUT_REQUEST,
      INVOCATION_REQUEST,
      INVOCATION_RESULT,
      APPLICATION_UPDATE,
      STATE_KEYS_REQUEST,
      STATE_KEYS_RESULT,
      END_FLAG
    };

    static constexpr uint16_t HEADER_OFFSET = 6;
    static constexpr uint16_t NAME_LENGTH = 32;
    static constexpr uint16_t ID_LENGTH = 16;
    static constexpr uint16_t BUF_SIZE = 128;

    std::array<int8_t, BUF_SIZE> data{};

    Message(Type type = Type::GENERIC_HEADER)
    {
      // NOLINTNEXTLINE
      *reinterpret_cast<int16_t*>(data.data()) = static_cast<int16_t>(type);
    }

    Type type() const;

    int32_t total_length() const;

    void total_length(int32_t);

    const int8_t* bytes() const
    {
      return data.data();
    }

    using MessageVariants = std::variant<
        GetRequestParsed, PutRequestParsed, InvocationRequestParsed, InvocationResultParsed,
        ApplicationUpdateParsed, StateKeysResultParsed, StateKeysRequestParsed>;

    MessageVariants parse() const;

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
    bool state() const;
  };

  struct GenericRequest : Message, GenericRequestParsed {

    GenericRequest(Type msg_type)
        : Message(msg_type), GenericRequestParsed(this->data.data() + HEADER_OFFSET)
    {
      // FIXME: - default options
      state(false);
    }

    void data_len(int32_t);
    void process_id(std::string_view);
    void name(std::string_view);
    void state(bool);
  };

  struct GetRequestParsed : GenericRequestParsed {

    GetRequestParsed(const int8_t* buf) : GenericRequestParsed(buf) {}
  };

  struct GetRequest : GenericRequest {

    GetRequest() : GenericRequest(Type::GET_REQUEST) {}

    using GenericRequest::data_len;
    using GenericRequest::name;
    using GenericRequest::process_id;
    using GenericRequest::state;
    using GenericRequestParsed::data_len;
    using GenericRequestParsed::name;
    using GenericRequestParsed::process_id;
    using GenericRequestParsed::state;

    static constexpr Type TYPE = Type::GET_REQUEST;
  };

  struct PutRequestParsed : GenericRequestParsed {

    PutRequestParsed(const int8_t* buf) : GenericRequestParsed(buf) {}
  };

  struct PutRequest : GenericRequest {

    PutRequest() : GenericRequest(Type::PUT_REQUEST) {}

    using GenericRequest::data_len;
    using GenericRequest::name;
    using GenericRequest::process_id;
    using GenericRequestParsed::data_len;
    using GenericRequestParsed::name;
    using GenericRequestParsed::process_id;

    static constexpr Type TYPE = Type::PUT_REQUEST;
  };

  struct InvocationRequestParsed {
    const int8_t* buf;
    size_t process_id_len;
    size_t id_len;
    size_t name_len;

    InvocationRequestParsed(const int8_t* buf)
        : buf(buf),
          // NOLINTNEXTLINE
          process_id_len(strnlen(
              reinterpret_cast<const char*>(buf + Message::ID_LENGTH + Message::NAME_LENGTH),
              Message::ID_LENGTH
          )),
          id_len(strnlen(reinterpret_cast<const char*>(buf), Message::ID_LENGTH)),
          name_len(strnlen(
              // NOLINTNEXTLINE
              reinterpret_cast<const char*>(buf + Message::ID_LENGTH), Message::NAME_LENGTH
          ))
    {
    }

    std::string_view process_id() const;
    std::string_view invocation_id() const;
    std::string_view function_name() const;
    int32_t buffers() const;
    const int32_t* buffers_lengths() const;
  };

  struct InvocationRequest : Message, InvocationRequestParsed {

    static constexpr int MAX_BUFFERS = 16;

    InvocationRequest()
        : Message(Type::INVOCATION_REQUEST),
          InvocationRequestParsed(this->data.data() + HEADER_OFFSET)
    {
    }

    using InvocationRequestParsed::buffers;
    using InvocationRequestParsed::function_name;
    using InvocationRequestParsed::invocation_id;
    using InvocationRequestParsed::process_id;

    void process_id(std::string_view id);
    void invocation_id(std::string_view id);
    void function_name(std::string_view name);
    void buffers(int32_t* begin, int32_t* end);
    void buffers(int32_t buf);

    template <typename Iter>
    void buffers(Iter begin, Iter end)
    {
      int elems = std::distance(begin, end);
      if (elems > InvocationRequest::MAX_BUFFERS) {
        throw common::InvalidArgument{fmt::format(
            "Number of buffers too large: {} > {}", elems, InvocationRequest::MAX_BUFFERS
        )};
      }

      // NOLINTNEXTLINE
      auto ptr = reinterpret_cast<int32_t*>(
          data.data() + HEADER_OFFSET + Message::NAME_LENGTH + 2 * Message::ID_LENGTH
      );
      *ptr++ = elems;

      while (begin != end) {
        *ptr++ = (*begin).len;
        ++begin;
      }
    }
  };

  struct InvocationResultParsed {
    const int8_t* buf;
    size_t id_len;

    InvocationResultParsed(const int8_t* buf)
        : buf(buf),
          // NOLINTNEXTLINE
          id_len(strnlen(reinterpret_cast<const char*>(buf), Message::ID_LENGTH))
    {
    }

    std::string_view invocation_id() const;
    int32_t buffer_length() const;
    int32_t return_code() const;
  };

  struct InvocationResult : Message, InvocationResultParsed {

    InvocationResult()
        : Message(Type::INVOCATION_RESULT),
          InvocationResultParsed(this->data.data() + HEADER_OFFSET)
    {
    }

    using InvocationResultParsed::buffer_length;
    using InvocationResultParsed::invocation_id;

    void invocation_id(std::string_view id);
    void buffer_length(int32_t length);
    void return_code(int32_t code);
  };

  struct StateKeysRequestParsed {
    const int8_t* buf;

    StateKeysRequestParsed(const int8_t* buf) : buf(buf) {}
  };

  struct StateKeysRequest : Message, StateKeysRequestParsed {

    StateKeysRequest()
        : Message(Type::STATE_KEYS_REQUEST),
          StateKeysRequestParsed(this->data.data() + HEADER_OFFSET)
    {
    }
  };

  struct StateKeysResultParsed {
    const int8_t* buf;

    StateKeysResultParsed(const int8_t* buf) : buf(buf) {}

    int32_t buffer_length() const;
  };

  struct StateKeysResult : Message, StateKeysResultParsed {

    StateKeysResult()
        : Message(Type::STATE_KEYS_RESULT), StateKeysResultParsed(this->data.data() + HEADER_OFFSET)
    {
    }

    using StateKeysResultParsed::buffer_length;

    void buffer_length(int32_t length);
  };

  struct ApplicationUpdateParsed {
    const int8_t* buf;
    size_t id_len;

    ApplicationUpdateParsed(const int8_t* buf)
        : buf(buf),
          // NOLINTNEXTLINE
          id_len(strnlen(reinterpret_cast<const char*>(buf), Message::NAME_LENGTH))
    {
    }

    std::string_view process_id() const;
    int32_t status_change() const;
  };

  struct ApplicationUpdate : Message, ApplicationUpdateParsed {

    ApplicationUpdate()
        : Message(Type::APPLICATION_UPDATE),
          ApplicationUpdateParsed(this->data.data() + HEADER_OFFSET)
    {
    }

    using ApplicationUpdateParsed::process_id;
    using ApplicationUpdateParsed::status_change;

    void process_id(std::string_view id);
    void status_change(int32_t code);
  };

} // namespace praas::process::runtime::internal::ipc

#endif
