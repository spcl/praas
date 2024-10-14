#ifndef PRAAS_COMMON_MESSAGES_HPP
#define PRAAS_COMMON_MESSAGES_HPP

#include <praas/common/exceptions.hpp>

#include <spdlog/fmt/bundled/core.h>

#include <array>
#include <cstring>
#include <memory>
#include <string>
#include <variant>

namespace praas::common::message {

  template <class... Ts>
  struct overloaded : Ts... {
    using Ts::operator()...;
  };
  template <class... Ts>
  overloaded(Ts...) -> overloaded<Ts...>;

  // struct ProcessConnectionParsed;
  // struct SwapRequestParsed;
  // struct SwapConfirmationParsed;
  // struct InvocationRequestParsed;
  // struct InvocationResultParsed;
  // struct DataPlaneMetricsParsed;
  // struct ProcessClosureParsed;
  // struct ApplicationUpdateParsed;
  // struct PutMessageParsed;

  // Connection Headers

  // Process connection
  // 2 bytes of identifier: 1
  // 32 bytes of process name
  // 34 bytes

  // Swap request message
  // 2 bytes of identifier
  // 40 bytes of length of swap path - prefix + app name
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

  struct MessageConfig {
    static constexpr uint16_t BUF_SIZE = 70;
    static constexpr uint16_t HEADER_OFFSET = 6;
    static constexpr uint16_t NAME_LENGTH = 32;
    static constexpr uint16_t ID_LENGTH = 16;
  };

  struct MessagePtr {
    const int8_t* ptr{};

    MessagePtr(const int8_t* ptr) : ptr(ptr) {}

    MessagePtr(const char* ptr)
    {
      // NOLINTNEXTLINE
      this->ptr = reinterpret_cast<const int8_t*>(ptr);
    }

    const int8_t* data() const
    {
      return ptr;
    }
  };

  struct MessageData {
    std::array<int8_t, MessageConfig::BUF_SIZE> buf{};

    MessageData& operator=(const MessagePtr& obj)
    {
      // NOLINTNEXTLINE
      std::copy(obj.data(), obj.data() + MessageConfig::BUF_SIZE, buf.data());
      return *this;
    }

    template <typename Message>
    MessageData& operator=(const Message& obj)
    {
      // NOLINTNEXTLINE
      std::copy(obj.bytes(), obj.bytes() + MessageConfig::BUF_SIZE, buf.data());
      return *this;
    }

    int8_t* data()
    {
      return buf.data();
    }

    const int8_t* data() const
    {
      return buf.data();
    }
  };

  enum class MessageType : int16_t {
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

  template <typename Data, template <class> typename CRTPMessageType>
  struct Message {

    static constexpr uint16_t BUF_SIZE = MessageConfig::BUF_SIZE;

    Data msg_data;

    Message(Data&& data, MessageType type = MessageType::GENERIC_HEADER) : msg_data(data)
    {
      if constexpr (std::is_same_v<Data, MessageData>) {
        // NOLINTNEXTLINE
        *reinterpret_cast<int16_t*>(bytes()) = static_cast<int16_t>(type);
      }
    }

    MessageType type() const
    {
      int16_t type = *reinterpret_cast<const int16_t*>(bytes());

      if (type >= static_cast<int16_t>(MessageType::END_FLAG) ||
          type < static_cast<int16_t>(MessageType::GENERIC_HEADER)) {
        throw common::InvalidMessage{fmt::format("Invalid type value for Message: {}", type)};
      }

      return static_cast<MessageType>(type);
    }

    void type(MessageType msg_type) {}

    int32_t total_length() const
    {
      // NOLINTNEXTLINE
      return *reinterpret_cast<const int32_t*>(bytes() + 2);
    }

    void total_length(int32_t len)
    {
      // NOLINTNEXTLINE
      *reinterpret_cast<int32_t*>(bytes() + 2) = len;
    }

    const int8_t* bytes() const
    {
      return msg_data.data();
    }

    template <typename D = Data, typename = std::enable_if_t<std::is_same_v<D, MessageData>>>
    int8_t* bytes()
    {
      return msg_data.data();
    }

    template <typename D = Data, typename = std::enable_if_t<std::is_same_v<D, MessageData>>>
    int8_t* data()
    {
      return msg_data.data() + MessageConfig::HEADER_OFFSET;
    }

    const int8_t* data() const
    {
      return msg_data.data() + MessageConfig::HEADER_OFFSET;
    }

    MessagePtr to_ptr() const
    {
      return MessagePtr{bytes()};
    }

    Data& data_buffer()
    {
      return msg_data;
    }

    CRTPMessageType<MessageData> copy()
    {
      CRTPMessageType<MessageData> copy{MessageData{}, type()};
      std::copy(bytes(), bytes() + BUF_SIZE, copy.bytes());
      return copy;
    }
  };

  template <typename Data>
  struct ProcessConnection : Message<Data, ProcessConnection> {

    using Parent = Message<Data, ProcessConnection>;
    using Parent::data;
    using Parent::data_buffer;
    using Parent::Parent;

    size_t process_name_len;

    ProcessConnection(Data&& data = Data())
        // NOLINTNEXTLINE
        : Parent(std::forward<Data>(data), MessageType::PROCESS_CONNECTION),
          process_name_len(
              strnlen(reinterpret_cast<const char*>(this->data()), MessageConfig::NAME_LENGTH)
          )
    {
    }

    std::string_view process_name() const
    {
      // NOLINTNEXTLINE
      return std::string_view{reinterpret_cast<const char*>(data()), process_name_len};
    }

    template <typename D = Data, typename = std::enable_if_t<std::is_same_v<D, MessageData>>>
    void process_name(std::string_view name)
    {
      if (name.length() > MessageConfig::NAME_LENGTH) {
        throw common::InvalidArgument{fmt::format(
            "Process name too long: {} > {}", name.length(), MessageConfig::NAME_LENGTH
        )};
      }
      std::strncpy(
          // NOLINTNEXTLINE
          reinterpret_cast<char*>(data()), name.data(), MessageConfig::NAME_LENGTH
      );
      process_name_len = name.length();
    }

    static MessageType type()
    {
      return MessageType::PROCESS_CONNECTION;
    }
  };

  template <typename Data>
  struct SwapRequest : Message<Data, SwapRequest> {

    using Parent = Message<Data, SwapRequest>;
    using Parent::data;
    using Parent::data_buffer;

    size_t path_len;

    SwapRequest(Data&& data = Data())
        // NOLINTNEXTLINE
        : Parent(std::forward<Data>(data), MessageType::SWAP_REQUEST),
          path_len(strnlen(reinterpret_cast<const char*>(this->data()), MessageConfig::NAME_LENGTH + 8))
    {
    }

    std::string_view path() const
    {
      // NOLINTNEXTLINE
      return std::string_view{reinterpret_cast<const char*>(data()), path_len};
    }

    void path(const std::string& path)
    {
      this->path(std::string_view{path});
    }

    void path(std::string_view path)
    {
      constexpr int max_size = MessageConfig::NAME_LENGTH + 8;
      if (path.length() > max_size) {
        throw common::InvalidArgument{fmt::format(
            "Swap location ID too long: {} > {}", path.length(), max_size 
        )};
      }
      std::strncpy(
          // NOLINTNEXTLINE
          reinterpret_cast<char*>(data()), path.data(), max_size
      );
      path_len = path.length();
    }

    static MessageType type()
    {
      return MessageType::SWAP_REQUEST;
    }
  };

  template <typename Data>
  struct SwapConfirmation : Message<Data, SwapConfirmation> {

    using Parent = Message<Data, SwapConfirmation>;
    using Parent::data;
    using Parent::data_buffer;

    SwapConfirmation(Data&& data = Data())
        // NOLINTNEXTLINE
        : Parent(std::forward<Data>(data), MessageType::SWAP_CONFIRMATION)
    {
    }

    double swap_time() const
    {
      // NOLINTNEXTLINE
      return *reinterpret_cast<const double*>(data() + 4);
    }

    void swap_time(double time)
    {
      // NOLINTNEXTLINE
      *reinterpret_cast<double*>(data() + 4) = time;
    }

    int32_t swap_size() const
    {
      // NOLINTNEXTLINE
      return *reinterpret_cast<const int32_t*>(data());
    }

    void swap_size(int32_t size)
    {
      // NOLINTNEXTLINE
      *reinterpret_cast<int32_t*>(data()) = size;
    }

    static MessageType type()
    {
      return MessageType::SWAP_CONFIRMATION;
    }
  };

  template <typename Data>
  struct InvocationRequest : Message<Data, InvocationRequest> {

    using Parent = Message<Data, InvocationRequest>;
    using Parent::data;
    using Parent::data_buffer;

    size_t fname_len;
    size_t invocation_id_len;

    InvocationRequest(Data&& data = Data())
        // NOLINTNEXTLINE
        : Parent(std::forward<Data>(data), MessageType::INVOCATION_REQUEST),
          fname_len(
              strnlen(reinterpret_cast<const char*>(this->data() + 4), MessageConfig::NAME_LENGTH)
          ),
          invocation_id_len(strnlen(
              reinterpret_cast<const char*>(this->data() + MessageConfig::NAME_LENGTH + 4),
              MessageConfig::ID_LENGTH
          ))
    {
    }

    void invocation_id(std::string_view name)
    {
      if (name.length() > MessageConfig::ID_LENGTH) {
        throw common::InvalidArgument{fmt::format(
            "Invocation ID too long: {} > {}", name.length(), MessageConfig::ID_LENGTH
        )};
      }
      std::strncpy(
          // NOLINTNEXTLINE
          reinterpret_cast<char*>(data() + MessageConfig::NAME_LENGTH + 4), name.data(),
          MessageConfig::ID_LENGTH
      );
      invocation_id_len = name.length();
    }

    std::string_view invocation_id() const
    {
      return std::string_view{
          // NOLINTNEXTLINE
          reinterpret_cast<const char*>(data() + MessageConfig::NAME_LENGTH + 4),
          invocation_id_len};
    }

    std::string_view function_name() const
    {
      // NOLINTNEXTLINE
      return std::string_view{reinterpret_cast<const char*>(data() + 4), fname_len};
    }

    void function_name(std::string_view name)
    {
      if (name.length() > MessageConfig::NAME_LENGTH) {
        throw common::InvalidArgument{fmt::format(
            "Function name too long: {} > {}", name.length(), MessageConfig::NAME_LENGTH
        )};
      }
      std::strncpy(
          // NOLINTNEXTLINE
          reinterpret_cast<char*>(data() + 4), name.data(), MessageConfig::NAME_LENGTH
      );
      fname_len = name.length();
    }

    int32_t payload_size() const
    {
      // NOLINTNEXTLINE
      return *reinterpret_cast<const int32_t*>(data());
    }

    void payload_size(int32_t size)
    {
      if (size < 0) {
        throw common::InvalidArgument{fmt::format("Payload size too small: {}", size)};
      }

      // NOLINTNEXTLINE
      *reinterpret_cast<int32_t*>(data()) = size;
    }

    static MessageType type()
    {
      return MessageType::INVOCATION_REQUEST;
    }
  };

  template <typename Data>
  struct InvocationResult : Message<Data, InvocationResult> {

    using Parent = Message<Data, InvocationResult>;
    using Parent::data;
    using Parent::data_buffer;

    size_t invocation_id_len;

    InvocationResult(Data&& data = Data())
        // NOLINTNEXTLINE
        : Parent(std::forward<Data>(data), MessageType::INVOCATION_RESULT),
          invocation_id_len(
              strnlen(reinterpret_cast<const char*>(this->data() + 4), MessageConfig::ID_LENGTH)
          )
    {
    }

    void invocation_id(std::string_view name)
    {
      if (name.length() > MessageConfig::ID_LENGTH) {
        throw common::InvalidArgument{fmt::format(
            "Invocation ID too long: {} > {}", name.length(), MessageConfig::ID_LENGTH
        )};
      }
      std::strncpy(
          // NOLINTNEXTLINE
          reinterpret_cast<char*>(data() + 4), name.data(), MessageConfig::ID_LENGTH
      );
      invocation_id_len = name.length();
    }

    std::string_view invocation_id() const
    {
      return std::string_view{// NOLINTNEXTLINE
                              reinterpret_cast<const char*>(data() + 4), invocation_id_len};
    }

    int32_t return_code() const
    {
      // NOLINTNEXTLINE
      return *reinterpret_cast<const int32_t*>(data());
    }

    void return_code(int32_t size)
    {
      // NOLINTNEXTLINE
      *reinterpret_cast<int32_t*>(data()) = size;
    }

    static MessageType type()
    {
      return MessageType::INVOCATION_RESULT;
    }
  };

  template <typename Data>
  struct DataPlaneMetrics : Message<Data, DataPlaneMetrics> {

    using Parent = Message<Data, DataPlaneMetrics>;
    using Parent::data;
    using Parent::data_buffer;

    DataPlaneMetrics(Data&& data = Data())
        // NOLINTNEXTLINE
        : Parent(std::forward<Data>(data), MessageType::DATAPLANE_METRICS)
    {
    }

    int32_t invocations() const
    {
      // NOLINTNEXTLINE
      return *reinterpret_cast<const int32_t*>(data());
    }

    void invocations(int32_t invocations)
    {
      if (invocations < 0) {
        throw common::InvalidArgument{
            fmt::format("Incorrect number of invocations {}", invocations)};
      }

      // NOLINTNEXTLINE
      *reinterpret_cast<int32_t*>(data()) = invocations;
    }

    uint64_t last_invocation_timestamp() const
    {
      // NOLINTNEXTLINE
      return *reinterpret_cast<const uint64_t*>(data() + 8);
    }

    void last_invocation_timestamp(uint64_t timestamp)
    {
      // NOLINTNEXTLINE
      *reinterpret_cast<uint64_t*>(data() + 8) = timestamp;
    }

    int32_t computation_time() const
    {
      // NOLINTNEXTLINE
      return *reinterpret_cast<const int32_t*>(data() + 4);
    }

    void computation_time(int32_t time)
    {
      if (time < 0) {
        throw common::InvalidArgument{fmt::format("Incorrect computation time {}", time)};
      }

      // NOLINTNEXTLINE
      *reinterpret_cast<int32_t*>(data() + 4) = time;
    }

    static MessageType type()
    {
      return MessageType::DATAPLANE_METRICS;
    }
  };

  template <typename Data>
  struct ProcessClosure : Message<Data, ProcessClosure> {

    using Parent = Message<Data, ProcessClosure>;
    using Parent::data;

    ProcessClosure(Data&& data = Data())
        // NOLINTNEXTLINE
        : Parent(std::forward<Data>(data), MessageType::PROCESS_CLOSURE)
    {
    }

    static MessageType type()
    {
      return MessageType::PROCESS_CLOSURE;
    }
  };

  template <typename Data>
  struct ApplicationUpdate : Message<Data, ApplicationUpdate> {

    using Parent = Message<Data, ApplicationUpdate>;
    using Parent::data;
    using Parent::data_buffer;

    size_t id_len;
    size_t ip_len;

    ApplicationUpdate(Data&& data = Data())
        // NOLINTNEXTLINE
        : Parent(std::forward<Data>(data), MessageType::APPLICATION_UPDATE),
          // NOLINTNEXTLINE
          id_len(strnlen(reinterpret_cast<const char*>(this->data()), MessageConfig::NAME_LENGTH)),
          ip_len(strnlen(
              reinterpret_cast<const char*>(this->data() + MessageConfig::NAME_LENGTH),
              MessageConfig::ID_LENGTH
          ))
    {
    }

    std::string_view process_id() const
    {
      // NOLINTNEXTLINE
      return std::string_view{reinterpret_cast<const char*>(data()), id_len};
    }

    void process_id(std::string_view id)
    {
      if (id.length() > MessageConfig::NAME_LENGTH) {
        throw common::InvalidArgument{
            fmt::format("Process ID too long: {} > {}", id.length(), MessageConfig::NAME_LENGTH)};
      }

      std::strncpy(
          // NOLINTNEXTLINE
          reinterpret_cast<char*>(data()), id.data(), MessageConfig::NAME_LENGTH
      );
      id_len = id.length();
    }

    void ip_address(std::string_view ip_addr)
    {
      if (ip_addr.length() > MessageConfig::ID_LENGTH) {
        throw common::InvalidArgument{fmt::format(
            "Process ID too long: {} > {}", ip_addr.length(), MessageConfig::ID_LENGTH
        )};
      }

      std::strncpy(
          // NOLINTNEXTLINE
          reinterpret_cast<char*>(data() + MessageConfig::NAME_LENGTH), ip_addr.data(),
          MessageConfig::ID_LENGTH
      );
      ip_len = ip_addr.length();
    }

    std::string_view ip_address() const
    {
      return std::string_view{// NOLINTNEXTLINE
                              reinterpret_cast<const char*>(data() + MessageConfig::NAME_LENGTH),
                              ip_len};
    }

    int32_t status_change() const
    {
      // NOLINTNEXTLINE
      return *reinterpret_cast<const int32_t*>(
          data() + MessageConfig::NAME_LENGTH + MessageConfig::ID_LENGTH
      );
    }

    void status_change(int32_t status)
    {
      // NOLINTNEXTLINE
      *reinterpret_cast<int32_t*>(data() + MessageConfig::NAME_LENGTH + MessageConfig::ID_LENGTH) =
          status;
    }

    int32_t port() const
    {
      // NOLINTNEXTLINE
      return *reinterpret_cast<const int32_t*>(
          data() + MessageConfig::NAME_LENGTH + MessageConfig::ID_LENGTH + sizeof(int32_t)
      );
    }

    void port(int32_t port)
    {
      // NOLINTNEXTLINE
      *reinterpret_cast<int32_t*>(
          data() + MessageConfig::NAME_LENGTH + MessageConfig::ID_LENGTH + sizeof(int32_t)
      ) = port;
    }

    static MessageType type()
    {
      return MessageType::APPLICATION_UPDATE;
    }
  };

  template <typename Data>
  struct PutMessage : Message<Data, PutMessage> {

    using Parent = Message<Data, PutMessage>;
    using Parent::data;
    using Parent::data_buffer;

    size_t name_len;
    size_t id_len;

    PutMessage(Data&& data = Data())
        // NOLINTNEXTLINE
        : Parent(std::forward<Data>(data), MessageType::PUT_MESSAGE),
          // NOLINTNEXTLINE
          name_len(strnlen(reinterpret_cast<const char*>(this->data()), MessageConfig::NAME_LENGTH)
          ),
          id_len(strnlen(
              reinterpret_cast<const char*>(this->data() + MessageConfig::NAME_LENGTH),
              MessageConfig::NAME_LENGTH
          ))
    {
    }

    void name(std::string_view name)
    {
      if (name.length() > MessageConfig::NAME_LENGTH) {
        throw common::InvalidArgument{fmt::format(
            "Message name too long: {} > {}", name.length(), MessageConfig::NAME_LENGTH
        )};
      }
      std::strncpy(
          // NOLINTNEXTLINE
          reinterpret_cast<char*>(data()), name.data(), MessageConfig::NAME_LENGTH
      );
      name_len = name.length();
    }

    std::string_view name() const
    {
      return std::string_view{// NOLINTNEXTLINE
                              reinterpret_cast<const char*>(data()), name_len};
    }

    void process_id(std::string_view name)
    {
      if (name.length() > MessageConfig::NAME_LENGTH) {
        throw common::InvalidArgument{fmt::format(
            "Invocation ID too long: {} > {}", name.length(), MessageConfig::NAME_LENGTH
        )};
      }
      std::strncpy(
          // NOLINTNEXTLINE
          reinterpret_cast<char*>(data() + MessageConfig::NAME_LENGTH), name.data(),
          MessageConfig::NAME_LENGTH
      );
      id_len = name.length();
    }

    std::string_view process_id() const
    {
      return std::string_view{// NOLINTNEXTLINE
                              reinterpret_cast<const char*>(data() + MessageConfig::NAME_LENGTH),
                              id_len};
    }

    static MessageType type()
    {
      return MessageType::PUT_MESSAGE;
    }
  };

  using ProcessConnectionData = ProcessConnection<MessageData>;
  using SwapRequestData = SwapRequest<MessageData>;
  using SwapConfirmationData = SwapConfirmation<MessageData>;
  using InvocationRequestData = InvocationRequest<MessageData>;
  using InvocationResultData = InvocationResult<MessageData>;
  using DataPlaneMetricsData = DataPlaneMetrics<MessageData>;
  using ProcessClosureData = ProcessClosure<MessageData>;
  using ApplicationUpdateData = ApplicationUpdate<MessageData>;
  using PutMessageData = PutMessage<MessageData>;
  using ProcessConnectionPtr = ProcessConnection<MessagePtr>;
  using SwapRequestPtr = SwapRequest<MessagePtr>;
  using SwapConfirmationPtr = SwapConfirmation<MessagePtr>;
  using InvocationRequestPtr = InvocationRequest<MessagePtr>;
  using InvocationResultPtr = InvocationResult<MessagePtr>;
  using DataPlaneMetricsPtr = DataPlaneMetrics<MessagePtr>;
  using ProcessClosurePtr = ProcessClosure<MessagePtr>;
  using ApplicationUpdatePtr = ApplicationUpdate<MessagePtr>;
  using PutMessagePtr = PutMessage<MessagePtr>;

  using MessageVariants = std::variant<
      std::monostate, ProcessConnectionPtr, SwapRequestPtr, SwapConfirmationPtr,
      InvocationRequestPtr, InvocationResultPtr, DataPlaneMetricsPtr, ProcessClosurePtr,
      ApplicationUpdatePtr, PutMessagePtr>;

  struct MessageParser {

    static MessageVariants parse(const MessageData& data)
    {
      return parse(MessagePtr{data.data()});
    }

    static MessageVariants parse(MessagePtr data)
    {
      int16_t type_val = *reinterpret_cast<const int16_t*>(data.ptr);
      if (type_val >= static_cast<int16_t>(MessageType::END_FLAG) ||
          type_val < static_cast<int16_t>(MessageType::GENERIC_HEADER)) {
        throw common::InvalidMessage{fmt::format("Invalid type value for Message: {}", type_val)};
      }
      auto type = static_cast<MessageType>(type_val);

      if (type == MessageType::PROCESS_CONNECTION) {
        return MessageVariants{ProcessConnection<MessagePtr>(std::move(data))};
      }

      if (type == MessageType::SWAP_REQUEST) {
        return MessageVariants{SwapRequestPtr(std::move(data))};
      }

      if (type == MessageType::SWAP_CONFIRMATION) {
        return MessageVariants{SwapConfirmationPtr(std::move(data))};
      }

      if (type == MessageType::INVOCATION_REQUEST) {
        return MessageVariants{InvocationRequestPtr(std::move(data))};
      }

      if (type == MessageType::INVOCATION_RESULT) {
        return MessageVariants{InvocationResultPtr(std::move(data))};
      }

      if (type == MessageType::DATAPLANE_METRICS) {
        return MessageVariants{DataPlaneMetricsPtr(std::move(data))};
      }

      if (type == MessageType::PROCESS_CLOSURE) {
        return MessageVariants{ProcessClosurePtr(std::move(data))};
      }

      if (type == MessageType::APPLICATION_UPDATE) {
        return MessageVariants{ApplicationUpdatePtr(std::move(data))};
      }

      if (type == MessageType::PUT_MESSAGE) {
        return MessageVariants{PutMessagePtr(std::move(data))};
      }

      throw common::NotImplementedError{};
    }
  };

} // namespace praas::common::message

#endif
