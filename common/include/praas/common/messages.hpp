#ifndef PRAAS_COMMON_MESSAGES_HPP
#define PRAAS_COMMON_MESSAGES_HPP

#include <memory>
#include <string.h>
#include <string>
#include <variant>

namespace praas::common::message {

  template <class... Ts>
  struct overloaded : Ts... {
    using Ts::operator()...;
  };
  template <class... Ts>
  overloaded(Ts...) -> overloaded<Ts...>;

  struct ProcessConnectionParsed;
  struct SwapRequestParsed;
  struct SwapConfirmationParsed;
  struct InvocationRequestParsed;
  struct InvocationResultParsed;
  struct DataPlaneMetricsParsed;
  struct ProcessClosureParsed;
  struct ApplicationUpdateParsed;
  struct PutMessageParsed;

  struct Message {

    enum class Type : int16_t {
      GENERIC_HEADER = 0,
      PROCESS_CONNECTION = 1,
      SWAP_REQUEST,
      SWAP_CONFIRMATION,
      INVOCATION_REQUEST,
      INVOCATION_RESULT,
      DATAPLANE_METRICS,
      PROCESS_CLOSURE,
      APPLICATION_UPDATE,
      PUT_MESSAGE,
      END_FLAG
    };

    // Connection Headers

    // Process connection
    // 2 bytes of identifier: 1
    // 32 bytes of process name
    // 34 bytes

    // Swap request message
    // 2 bytes of identiifer
    // 4 bytes of length of swap path
    // Message is followed by the swap path

    // Swap confirmation
    // 2 bytes of identifier: 2
    // 4 bytes of swap size
    // 6 bytes

    // Invocation request
    // 2 bytes of identifier: 3
    // 16 bytes of invocation id
    // 32 bytes of function name
    // 8 bytes payload length
    // 58 bytes

    // Invocation response
    // 2 bytes of identifier: 4
    // 32 bytes of invocation id
    // 8 bytes response size
    // 42 bytes

    // Data plane metrics
    // 2 bytes of identifier: 5
    // 4 bytes of invocations
    // 4 bytes of computation time
    // 8 bytes last invocation
    // 18 bytes

    // Closure of process
    // 2 bytes of identifier: 6
    // 2 bytes

    static constexpr uint16_t HEADER_OFFSET = 6;
    static constexpr uint16_t NAME_LENGTH = 32;
    static constexpr uint16_t ID_LENGTH = 16;
    static constexpr uint16_t BUF_SIZE = 70;
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
        ProcessConnectionParsed, SwapRequestParsed, SwapConfirmationParsed, InvocationRequestParsed,
        InvocationResultParsed, DataPlaneMetricsParsed, ProcessClosureParsed, ApplicationUpdateParsed,
        PutMessageParsed
    >;

    MessageVariants parse();
    static MessageVariants parse_message(const int8_t* data);
    static MessageVariants parse_message(const char* data);
  };

  struct ProcessConnectionParsed {
    static constexpr uint16_t EXPECTED_LENGTH = 34;
    const int8_t* buf{};
    size_t process_name_len;

    ProcessConnectionParsed(const int8_t* buf)
        // NOLINTNEXTLINE
        : buf(buf),
          process_name_len(strnlen(reinterpret_cast<const char*>(buf), Message::NAME_LENGTH))
    {
    }

    std::string_view process_name() const;

    static Message::Type type();
  };

  struct ProcessConnection : Message, ProcessConnectionParsed {
    static constexpr uint16_t EXPECTED_LENGTH = 34;

    ProcessConnection()
        : Message(Type::PROCESS_CONNECTION),
          ProcessConnectionParsed(this->data.data() + HEADER_OFFSET)
    {
      static_assert(EXPECTED_LENGTH <= BUF_SIZE);
    }

    using ProcessConnectionParsed::process_name;
    void process_name(std::string_view name);

    using ProcessConnectionParsed::type;
  };

  struct SwapRequestParsed {
    const int8_t* buf;
    size_t path_len;

    SwapRequestParsed(const int8_t* buf)
        : buf(buf),
          // NOLINTNEXTLINE
          path_len(strnlen(reinterpret_cast<const char*>(buf), Message::ID_LENGTH))
    {}

    std::string_view path() const;

    static Message::Type type();
  };

  struct SwapRequest : Message, SwapRequestParsed {

    SwapRequest()
        : Message(Type::SWAP_REQUEST), SwapRequestParsed(this->data.data() + HEADER_OFFSET)
    {}

    using SwapRequestParsed::type;
    using SwapRequestParsed::path;

    void path(const std::string& function_name);
    void path(std::string_view function_name);
  };

  struct SwapConfirmationParsed {
    const int8_t* buf;

    SwapConfirmationParsed(const int8_t* buf) : buf(buf) {}

    int32_t swap_size() const;

    static Message::Type type();
  };

  struct SwapConfirmation : Message, SwapConfirmationParsed {

    SwapConfirmation()
        : Message(Type::SWAP_CONFIRMATION),
          SwapConfirmationParsed(this->data.data() + HEADER_OFFSET)
    {}

    using SwapConfirmationParsed::type;
    using SwapConfirmationParsed::swap_size;

    void swap_size(int32_t);
  };

  struct InvocationRequestParsed {
    const int8_t* buf;
    size_t fname_len;
    size_t invocation_id_len;

    InvocationRequestParsed(const int8_t* buf)
        // NOLINTNEXTLINE
        : buf(buf),
          fname_len(strnlen(reinterpret_cast<const char*>(buf + 4), Message::NAME_LENGTH)),
          invocation_id_len(strnlen(
              reinterpret_cast<const char*>(buf + Message::NAME_LENGTH + 4), Message::ID_LENGTH
          ))
    {
    }

    std::string_view invocation_id() const;
    std::string_view function_name() const;
    int32_t payload_size() const;

    static Message::Type type();
  };

  struct InvocationRequest : Message, InvocationRequestParsed {
    static constexpr uint16_t EXPECTED_LENGTH = 58;

    InvocationRequest()
        : Message(Type::INVOCATION_REQUEST),
          InvocationRequestParsed(this->data.data() + HEADER_OFFSET)
    {
      static_assert(EXPECTED_LENGTH <= BUF_SIZE);
    }

    using InvocationRequestParsed::function_name;
    using InvocationRequestParsed::invocation_id;
    using InvocationRequestParsed::payload_size;
    using InvocationRequestParsed::type;

    void invocation_id(std::string_view invocation_id);
    void function_name(std::string_view function_name);
    void payload_size(int32_t);
  };

  struct InvocationResultParsed {
    const int8_t* buf;
    size_t invocation_id_len;

    InvocationResultParsed(const int8_t* buf)
        // NOLINTNEXTLINE
        : buf(buf),
          invocation_id_len(strnlen(reinterpret_cast<const char*>(buf), Message::NAME_LENGTH))
    {
    }

    std::string_view invocation_id() const;
    int32_t return_code() const;
    // FIXME: common parent
    int32_t total_length() const;
    static Message::Type type();
  };

  struct InvocationResult : Message, InvocationResultParsed {
    static constexpr uint16_t EXPECTED_LENGTH = 42;

    InvocationResult()
        : Message(Type::INVOCATION_RESULT),
          InvocationResultParsed(this->data.data() + HEADER_OFFSET)
    {
      static_assert(EXPECTED_LENGTH <= BUF_SIZE);
    }

    using InvocationResultParsed::invocation_id;
    using InvocationResultParsed::return_code;
    using InvocationResultParsed::type;
    using Message::total_length;

    void invocation_id(std::string_view invocation_id);
    void return_code(int32_t);
  };

  struct DataPlaneMetricsParsed {
    const int8_t* buf;

    DataPlaneMetricsParsed(const int8_t* buf) : buf(buf) {}

    // 4 bytes of invocations
    // 4 bytes of computation time
    // 8 bytes last invocation
    int32_t invocations() const;
    int32_t computation_time() const;
    uint64_t last_invocation_timestamp() const;
    static Message::Type type();
  };

  struct DataPlaneMetrics : Message, DataPlaneMetricsParsed {
    static constexpr uint16_t EXPECTED_LENGTH = 18;

    DataPlaneMetrics()
        : Message(Type::DATAPLANE_METRICS),
          DataPlaneMetricsParsed(this->data.data() + HEADER_OFFSET)
    {
      static_assert(EXPECTED_LENGTH <= BUF_SIZE);
    }

    using DataPlaneMetricsParsed::computation_time;
    using DataPlaneMetricsParsed::invocations;
    using DataPlaneMetricsParsed::last_invocation_timestamp;
    using DataPlaneMetricsParsed::type;

    void invocations(int32_t);
    void computation_time(int32_t);
    void last_invocation_timestamp(uint64_t);
  };

  struct ProcessClosureParsed {

    ProcessClosureParsed(const int8_t* /* unused */ = nullptr) {}

    static Message::Type type();
  };

  struct ProcessClosure : Message, ProcessClosureParsed {

    ProcessClosure() : Message(Type::PROCESS_CLOSURE) {}

    using ProcessClosureParsed::type;
  };

  struct ApplicationUpdateParsed {
    const int8_t* buf;
    size_t id_len;
    size_t ip_len;

    ApplicationUpdateParsed(const int8_t* buf)
        : buf(buf),
          // NOLINTNEXTLINE
          id_len(strnlen(reinterpret_cast<const char*>(buf), Message::NAME_LENGTH)),
          ip_len(strnlen(reinterpret_cast<const char*>(buf + Message::NAME_LENGTH), Message::ID_LENGTH))
    {
    }

    std::string_view process_id() const;
    std::string_view ip_address() const;
    int32_t status_change() const;
    int32_t port() const;
  };

  struct ApplicationUpdate : Message, ApplicationUpdateParsed {

    ApplicationUpdate()
        : Message(Type::APPLICATION_UPDATE),
          ApplicationUpdateParsed(this->data.data() + HEADER_OFFSET)
    {
    }

    using ApplicationUpdateParsed::status_change;
    using ApplicationUpdateParsed::process_id;
    using ApplicationUpdateParsed::ip_address;
    using ApplicationUpdateParsed::port;

    void process_id(std::string_view id);
    void ip_address(std::string_view id);
    void status_change(int32_t code);
    void port(int32_t code);
  };

  struct PutMessageParsed {
    const int8_t* buf;
    size_t name_len;
    size_t id_len;

    PutMessageParsed(const int8_t* buf)
        : buf(buf),
          // NOLINTNEXTLINE
          name_len(strnlen(reinterpret_cast<const char*>(buf), Message::NAME_LENGTH)),
          id_len(strnlen(reinterpret_cast<const char*>(buf + Message::NAME_LENGTH), Message::NAME_LENGTH))
    {
    }

    std::string_view name() const;
    std::string_view process_id() const;
    //int32_t payload_size() const;
    // FIXME: common parent
    int32_t total_length() const;
  };

  struct PutMessage : Message, PutMessageParsed {

    PutMessage()
        : Message(Type::PUT_MESSAGE),
          PutMessageParsed(this->data.data() + HEADER_OFFSET)
    {
    }

    using PutMessageParsed::name;
    //using PutMessageParsed::payload_size;
    using Message::total_length;
    using PutMessageParsed::process_id;

    void name(std::string_view id);
    void process_id(std::string_view id);
    //void payload_size(int32_t);
  };

} // namespace praas::common::message

#endif
