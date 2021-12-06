
#ifndef __COMMON_MESSAGES_HPP__
#define __COMMON_MESSAGES_HPP__

#include <memory>
#include <string>
#include <cstring>

namespace praas::common {

  struct MessageType
  {
    enum class Type {
      CLIENT = 0,
      PROCESS = 1,
      SESSION = 2
    };

    virtual Type type() const = 0;
  };

  struct Header
  {
    // Connection Headers

    // Client connection
    // 2 bytes of identifier: 0
    // 4 bytes of packet size
    // 16 bytes of process name
    // 16 bytes of function name
    // 16 bytes of session id
    // 54 bytes

    // Process connection
    // 2 bytes of identifier: 1
    // 16 bytes of process id
    // 18 bytes

    // Session connection
    // 2 bytes of identifier: 2
    // 16 bytes of session id
    // 18 bytes

    static constexpr uint16_t BUF_SIZE = 54;
    int8_t data[BUF_SIZE];

    std::unique_ptr<MessageType> parse(ssize_t);
  };

  struct ClientMessage: MessageType {
    static constexpr uint16_t EXPECTED_LENGTH = 54;
    int8_t* buf;

    ClientMessage(int8_t* buf):
      buf(buf)
    {}

    int32_t payload_size();
    std::string process_name();
    std::string function_name();
    std::string session_id();
    Type type() const override;
  };

  struct ProcessMessage: MessageType {
    static constexpr uint16_t EXPECTED_LENGTH = 18;
    int8_t* buf;

    ProcessMessage(int8_t* buf):
      buf(buf)
    {}

    std::string process_id();
    Type type() const override;
  };

  struct Request {
    enum class Type : int16_t {
      PROCESS_ALLOCATION = 11,
      SESSION_ALLOCATION = 12,
      FUNCTION_INVOCATION = 13
    };

    static constexpr uint16_t REQUEST_BUF_SIZE = 39;
    int8_t data[REQUEST_BUF_SIZE];

    Request()
    {
      memset(data, 0, REQUEST_BUF_SIZE);
    }
  };

  struct ProcessRequest : Request {

    // Process Allocation
    // 2 bytes identifier
    // 2 bytes max_sessions
    // 4 bytes port
    // 15 bytes IP address
    // 16 bytes process_id
    // 39 bytes

    using Request::REQUEST_BUF_SIZE;
    using Request::data;

    static constexpr uint16_t MSG_SIZE = 39;
    int16_t max_sessions();
    int32_t port();
    std::string ip_address();
    std::string process_id();

    ssize_t fill(int16_t sessions, int32_t port, std::string ip_address, std::string process_id);
  };

  struct SessionRequest : Request {

    // Session Allocation
    // 2 bytes of identifier
    // 4 bytes max functions
    // 4 bytes memory size
    // 16 bytes of session id
    // 26 bytes

    using Request::REQUEST_BUF_SIZE;
    using Request::data;

    static constexpr uint16_t MSG_SIZE = 26;
    int16_t max_functions();
    int32_t memory_size();
    std::string session_id();

    ssize_t fill(std::string session_id, int32_t max_functions, int32_t memory_size);
  };

}

#endif

