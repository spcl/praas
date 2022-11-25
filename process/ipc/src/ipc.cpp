#include <praas/process/ipc/ipc.hpp>

#include <praas/common/util.hpp>
#include <iostream>
#include <thread>
#include "praas/common/exceptions.hpp"

namespace praas::process::ipc {

  IPCMode deserialize(std::string mode)
  {
    if (mode == "posix_mq")
      return IPCMode::POSIX_MQ;
    else
      return IPCMode::NONE;
  }

  IPCChannel::~IPCChannel() {}

  POSIXMQChannel::POSIXMQChannel(
      std::string queue_name, IPCDirection direction, bool create, int message_size
  )
      : _created(create), _name(queue_name), _buffers(BUFFER_ELEMS, BUFFER_SIZE),
        _msg_size(message_size)
  {

    int mq_direction = direction == IPCDirection::WRITE ? O_WRONLY : O_RDONLY;

    if (create) {

      struct mq_attr attributes = {
          .mq_flags = 0, .mq_maxmsg = MAX_MSGS, .mq_msgsize = message_size, .mq_curmsgs = 0};

      std::cerr << attributes.mq_msgsize << std::endl;
      common::util::assert_other(
          _queue = mq_open(
              queue_name.c_str(), O_CREAT | O_EXCL | O_NONBLOCK | mq_direction, S_IRUSR | S_IWUSR,
              &attributes
          ),
          -1
      );
      std::cerr << attributes.mq_msgsize << std::endl;

    } else {

      common::util::assert_other(_queue = mq_open(queue_name.c_str(), mq_direction), -1);

      struct mq_attr attributes {};
      mq_getattr(_queue, &attributes);

      _msg_size = attributes.mq_msgsize;

      std::cerr << attributes.mq_msgsize << std::endl;
    }

    _msg_buffer.reset(new int8_t[_msg_size]);

    spdlog::info("Opened message queue {}", _name);
  }

  POSIXMQChannel::~POSIXMQChannel()
  {
    if (_created) {
      common::util::assert_other(mq_unlink(_name.c_str()), -1);

      spdlog::info("Closed message queue {}", _name);
    }
  }

  int POSIXMQChannel::fd() const
  {
    // "On Linux, a message queue descriptor is actually a file
    // descriptor.  (POSIX does not require such an implementation.)
    return _queue;
  }

  void POSIXMQChannel::send(Message& msg, std::initializer_list<Buffer<char>> data)
  {
    size_t len = 0;
    for (auto buf : data) {
      len += buf.len;
    }

    msg.total_length(len);
    std::cerr << len << std::endl;

    for (int i = 0; i < 64; ++i)
      spdlog::info(fmt::format("Byte {}, byte {:b}", i, msg.bytes()[i]));

    _send(msg.bytes(), msg.BUF_SIZE);
    for (auto buf : data) {
      _send(buf.val, buf.len);
    }
  }

  void POSIXMQChannel::_send(const int8_t* data, int len) const
  {
    // NOLINTNEXTLINE
    _send(reinterpret_cast<const char*>(data), len);
  }

  void POSIXMQChannel::_send(const char* data, int len) const
  {
    spdlog::error("Sending {} bytes on queue {}", len, _name);

    for (int pos = 0; pos < len;) {

      auto size = (len - pos < _msg_size) ? len - pos : _msg_size;
      int ret = mq_send(_queue, data + pos, size, 1);
      spdlog::info("Send {} at pos {} out of {}", ret, pos, size);

      if (ret == -1) {
        if (errno == EAGAIN) {
          std::this_thread::sleep_for(std::chrono::microseconds(1));
        } else {
          throw praas::common::PraaSException{
              fmt::format("Failed sending with error {}, strerror {}", errno, strerror(errno))};
        }
      } else {
        pos += size;
      }
    }
  }

  std::tuple<Message, Buffer<char>> POSIXMQChannel::receive()
  {
    Message msg;
    _recv(_msg_buffer.get(), Message::BUF_SIZE);
    // FIXME: avoid a copy here?
    std::copy_n(_msg_buffer.get(), Message::BUF_SIZE, msg.data.data());

    auto buf = _buffers.retrieve_buffer(msg.total_length());
    _recv(buf.val, buf.len);

    return std::make_tuple(msg, buf);
  }

  void POSIXMQChannel::_recv(int8_t* data, int len) const
  {

    // NOLINTNEXTLINE
    _recv(reinterpret_cast<char*>(data), len);

  }

  void POSIXMQChannel::_recv(char* data, int len) const
  {
    int received_length = 0;
    for (int pos = 0; pos < len;) {

      //auto size = (len - pos < _msg_size) ? (len - pos) : _msg_size;
      //spdlog::error("attempt Receive {} out of {} {}", size, len, _msg_size);
      long rcv_len = mq_receive(_queue, data + pos, _msg_size, nullptr);

      //std::cerr << fmt::format("Failed sending with error {}, strerror {}", errno, strerror(errno)) << std::endl;
      if (rcv_len == -1) {
        throw praas::common::PraaSException{
            fmt::format("Failed receiving with error {}, strerror {}", errno, strerror(errno))};
      } else {
        pos += rcv_len;
      }

    }
  }

} // namespace praas::process::ipc
