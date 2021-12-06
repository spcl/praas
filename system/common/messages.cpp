
#include <cstring>

#include "messages.hpp"

namespace praas::common {

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

  int16_t SessionRequest::max_functions()
  {
    return *reinterpret_cast<int16_t*>(data + 2);
  }

  int32_t SessionRequest::memory_size()
  {
    return *reinterpret_cast<int32_t*>(data + 4);
  }

  std::string SessionRequest::session_id()
  {
    return std::string{reinterpret_cast<char*>(data + 10), 16};
  }
  
  ssize_t SessionRequest::fill(
    std::string session_id, int32_t max_functions, int32_t memory_size
  )
  {
    *reinterpret_cast<int16_t*>(data) = static_cast<int16_t>(Request::Type::SESSION_ALLOCATION);
    *reinterpret_cast<int32_t*>(data + 2) = max_functions;
    *reinterpret_cast<int32_t*>(data + 6) = memory_size;
    std::strncpy(reinterpret_cast<char*>(data + 10), session_id.data(), 16);
    return MSG_SIZE;
  }

  int16_t ProcessRequest::max_sessions()
  {
    return *reinterpret_cast<int16_t*>(data + 2);
  }

  int32_t ProcessRequest::port()
  {
    return *reinterpret_cast<int32_t*>(data + 4);
  }

  std::string ProcessRequest::ip_address()
  {
    return std::string{reinterpret_cast<char*>(data + 8), 15};
  }

  std::string ProcessRequest::process_id()
  {
    return std::string{reinterpret_cast<char*>(data + 23), 16};
  }

  ssize_t ProcessRequest::fill(
      int16_t sessions, int32_t port, std::string ip_address, std::string process_id
  )
  {
    *reinterpret_cast<int16_t*>(data) = static_cast<int16_t>(Request::Type::PROCESS_ALLOCATION);
    *reinterpret_cast<int16_t*>(data + 2) = sessions;
    *reinterpret_cast<int32_t*>(data + 4) = port;
    std::strncpy(reinterpret_cast<char*>(data + 8), ip_address.data(), 15);
    std::strncpy(reinterpret_cast<char*>(data + 23), process_id.data(), 16);
    return MSG_SIZE;
  }

};
