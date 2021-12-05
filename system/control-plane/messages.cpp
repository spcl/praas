
#include "messages.hpp"

namespace praas::control_plane {

  std::unique_ptr<MessageType> Header::parse(ssize_t data_size)
  {
    int16_t type = *reinterpret_cast<int16_t*>(data);
    if(data_size == ClientMessage::EXPECTED_LENGTH && type == 0) {
      return std::make_unique<ClientMessage>(data + 2);
    } else if (data_size == ProcessMessage::EXPECTED_LENGTH && type == 1) {
      return std::make_unique<ProcessMessage>(data + 2);
    } else {
      return nullptr;
    }
  }

  int32_t ClientMessage::payload_size()
  {
    return *reinterpret_cast<int32_t*>(buf);
  }

  std::string ClientMessage::function_name()
  {
    return std::string{reinterpret_cast<char*>(buf + 20)};
  }

  std::string ClientMessage::process_name()
  {
    return std::string{reinterpret_cast<char*>(buf + 4)};
  }

  std::string ClientMessage::session_id()
  {
    return std::string{reinterpret_cast<char*>(buf + 36)};
  }

  MessageType::Type ClientMessage::type() const
  {
    return MessageType::Type::CLIENT; 
  }

  std::string ProcessMessage::process_id()
  {
    return std::string{reinterpret_cast<char*>(buf), 16};
  }

  MessageType::Type ProcessMessage::type() const
  {
    return MessageType::Type::PROCESS;
  }

};

