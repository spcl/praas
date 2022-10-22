
#ifndef PRAAS_COMMON_MESSAGES_HPP
#define PRAAS_COMMON_MESSAGES_HPP

#include <cstring>
#include <memory>
#include <string>

namespace praas::common {

  struct MessageType {
    enum class Type : int16_t {
      PROCESS_CONNECTION = 1,
      SWAP_CONFIRMATION = 2,
      INVOCATION_RESPONSE = 3,
      DATAPLANE_METRICS = 4,
      PROCESS_CLOSURE = 5
    };

    MessageType() = default;
    MessageType(const MessageType&) = default;
    MessageType(MessageType&&) = delete;
    MessageType& operator=(const MessageType&) = default;
    MessageType& operator=(MessageType&&) = delete;
    virtual ~MessageType() = default;

    virtual Type type() const = 0;
  };

  struct Header {

    static constexpr uint16_t NAME_LENGTH = 32;

    // Connection Headers

    // Process connection
    // 2 bytes of identifier: 1
    // 32 bytes of process name
    // 34 bytes

    // Swap confirmation
    // 2 bytes of identifier: 2
    // 2 bytes

    // Invocation response
    // 2 bytes of identifier: 3
    // 32 bytes of invocation id
    // 8 bytes response size
    // 42 bytes

    // Data plane metrics
    // 2 bytes of identifier: 4
    // 4 bytes of invocations
    // 4 bytes of computation time
    // 8 bytes last invocation
    // 18 bytes

    // Closure of process
    // 2 bytes of identifier: 5
    // 2 bytes

    static constexpr uint16_t BUF_SIZE = 42;
    std::array<int8_t, BUF_SIZE> data;

    std::unique_ptr<MessageType> parse();
  };

  struct ProcessConnectionMessage : MessageType {
    static constexpr uint16_t EXPECTED_LENGTH = 34;
    int8_t* buf;

    ProcessConnectionMessage(int8_t* buf) : buf(buf) {}

    std::string process_name() const;
    Type type() const override;
  };

  struct SwapConfirmation : MessageType {

    SwapConfirmation(int8_t*) {}

    Type type() const override;
  };

  struct InvocationResponseMessage : MessageType {
    int8_t* buf;

    InvocationResponseMessage(int8_t* buf) : buf(buf) {}

    std::string invocation_id() const;
    int32_t response_size() const;
    Type type() const override;
  };

  struct DataPlaneMetricsMessage : MessageType {
    int8_t* buf;

    DataPlaneMetricsMessage(int8_t* buf) : buf(buf) {}

    // 4 bytes of invocations
    // 4 bytes of computation time
    // 8 bytes last invocation
    int32_t invocations() const;
    int32_t computation_time() const;
    uint64_t last_invocation_timestamp() const;
    Type type() const override;
  };

  struct ClosureMessage : MessageType {

    ClosureMessage(int8_t*) {}

    Type type() const override;
  };

  struct Request {
    enum class Type : int16_t {
      PROCESS_ALLOCATION = 11,
      SESSION_ALLOCATION = 12,
      FUNCTION_INVOCATION = 13
    };

    static constexpr uint16_t EXPECTED_LENGTH = 39;
    std::array<int8_t, EXPECTED_LENGTH> data;

    Request(): data() { }
  };

  //struct ProcessRequest : Request {

  //  // Process Allocation
  //  // 2 bytes identifier
  //  // 2 bytes max_sessions
  //  // 4 bytes port
  //  // 15 bytes IP address
  //  // 16 bytes process_id
  //  // 39 bytes

  //  using Request::data;
  //  using Request::EXPECTED_LENGTH;

  //  int16_t max_sessions();
  //  int32_t port();
  //  std::string ip_address();
  //  std::string process_id();

  //  ssize_t fill(
  //      int16_t sessions, int32_t port, std::string ip_address,
  //      std::string process_id
  //  );
  //};

  //struct SessionRequest : Request {

  //  // Session Allocation
  //  // 2 bytes of identifier
  //  // 4 bytes max functions
  //  // 4 bytes memory size
  //  // 16 bytes of session id
  //  // 26 bytes

  //  using Request::data;
  //  using Request::EXPECTED_LENGTH;

  //  int16_t max_functions();
  //  int32_t memory_size();
  //  std::string session_id();

  //  ssize_t
  //  fill(std::string session_id, int32_t max_functions, int32_t memory_size);
  //};

  //struct FunctionRequest : Request {

  //  // Session Allocation
  //  // 2 bytes of identifier
  //  // 4 bytes payload size
  //  // 16 bytes of function name
  //  // 16 bytes of invocation id
  //  // 32 bytes

  //  using Request::data;
  //  using Request::EXPECTED_LENGTH;

  //  ssize_t fill(
  //      std::string function_name, std::string function_id, int32_t payload_size
  //  );
  //};

} // namespace praas::common

#endif
