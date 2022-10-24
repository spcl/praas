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
  struct SwapConfirmationParsed;
  struct InvocationRequestParsed;
  struct InvocationResultParsed;
  struct DataPlaneMetricsParsed;
  struct ProcessClosureParsed;

  struct Message {

    enum class Type : int16_t {
      GENERIC_HEADER = 0,
      PROCESS_CONNECTION = 1,
      SWAP_CONFIRMATION = 2,
      INVOCATION_REQUEST = 3,
      INVOCATION_RESULT = 4,
      DATAPLANE_METRICS = 5,
      PROCESS_CLOSURE = 6,
      END_FLAG = 7
    };

    // Connection Headers

    // Process connection
    // 2 bytes of identifier: 1
    // 32 bytes of process name
    // 34 bytes

    // Swap request message
    // FIXME:

    // Swap confirmation
    // 2 bytes of identifier: 2
    // 2 bytes

    // Invocation request
    // 2 bytes of identifier: 3
    // 32 bytes of invocation id
    // 8 bytes payload length
    // 42 bytes

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

    static constexpr uint16_t HEADER_OFFSET = 2;
    static constexpr uint16_t NAME_LENGTH = 32;
    static constexpr uint16_t BUF_SIZE = 42;
    std::array<int8_t, BUF_SIZE> data{};

    Message(Type type = Type::GENERIC_HEADER)
    {
      // NOLINTNEXTLINE
      *reinterpret_cast<int16_t*>(data.data()) = static_cast<int16_t>(type);
    }

    Type type() const;

    using MessageVariants = std::variant<
        ProcessConnectionParsed, SwapConfirmationParsed, InvocationRequestParsed,
        InvocationResultParsed, DataPlaneMetricsParsed, ProcessClosureParsed>;

    MessageVariants parse();
  };

  struct ProcessConnectionParsed {
    static constexpr uint16_t EXPECTED_LENGTH = 34;
    int8_t* buf{};
    size_t process_name_len;

    ProcessConnectionParsed(int8_t* buf)
        // NOLINTNEXTLINE
        : buf(buf), process_name_len(strnlen(reinterpret_cast<char*>(buf), Message::NAME_LENGTH))
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
    void process_name(const std::string& name);

    using ProcessConnectionParsed::type;
  };

  struct SwapConfirmationParsed {

    SwapConfirmationParsed(int8_t* /*unused*/ = nullptr) {}

    static Message::Type type();
  };

  struct SwapConfirmation : Message, SwapConfirmationParsed {

    SwapConfirmation() : Message(Type::SWAP_CONFIRMATION) {}

    using SwapConfirmationParsed::type;
  };

  struct InvocationRequestParsed {
    int8_t* buf;
    size_t invocation_id_len;

    InvocationRequestParsed(int8_t* buf)
        // NOLINTNEXTLINE
        : buf(buf), invocation_id_len(strnlen(reinterpret_cast<char*>(buf), Message::NAME_LENGTH))
    {
    }

    std::string_view invocation_id() const;
    int32_t payload_size() const;

    static Message::Type type();
  };

  struct InvocationRequest : Message, InvocationRequestParsed {
    static constexpr uint16_t EXPECTED_LENGTH = 42;

    InvocationRequest()
        : Message(Type::INVOCATION_REQUEST),
          InvocationRequestParsed(this->data.data() + HEADER_OFFSET)
    {
      static_assert(EXPECTED_LENGTH <= BUF_SIZE);
    }

    using InvocationRequestParsed::invocation_id;
    using InvocationRequestParsed::payload_size;
    using InvocationRequestParsed::type;

    void invocation_id(const std::string& invocation_id);
    void payload_size(int32_t);
  };

  struct InvocationResultParsed {
    int8_t* buf;
    size_t invocation_id_len;

    InvocationResultParsed(int8_t* buf)
        // NOLINTNEXTLINE
        : buf(buf), invocation_id_len(strnlen(reinterpret_cast<char*>(buf), Message::NAME_LENGTH))
    {
    }

    std::string_view invocation_id() const;
    int32_t response_size() const;
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
    using InvocationResultParsed::response_size;
    using InvocationResultParsed::type;

    void invocation_id(const std::string& invocation_id);
    void response_size(int32_t);
  };

  struct DataPlaneMetricsParsed {
    int8_t* buf;

    DataPlaneMetricsParsed(int8_t* buf) : buf(buf) {}

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

    ProcessClosureParsed(int8_t* /* unused */ = nullptr) {}

    static Message::Type type();
  };

  struct ProcessClosure : Message, ProcessClosureParsed {

    ProcessClosure() : Message(Type::PROCESS_CLOSURE) {}

    using ProcessClosureParsed::type;
  };

} // namespace praas::common::message

#endif
