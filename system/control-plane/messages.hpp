
#ifndef __CONTROLL_PLANE_MESSAGES_HPP__
#define __CONTROLL_PLANE_MESSAGES_HPP__

#include <memory>
#include <string>
namespace praas::control_plane {

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

}

#endif

