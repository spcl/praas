#include <praas/process/ipc/ipc.hpp>

#include <praas/common/util.hpp>

namespace praas::process::ipc {

  IPCMode deserialize(std::string mode)
  {
    if (mode == "posix_mq")
      return IPCMode::POSIX_MQ;
    else
      return IPCMode::NONE;
  }

  IPCChannel::~IPCChannel() {}

  POSIXMQChannel::POSIXMQChannel(std::string queue_name, bool create, int message_size)
      : _created(create), _name(queue_name), _buffers(BUFFER_ELEMS, BUFFER_SIZE), _msg_size(message_size)
  {

    if(create) {

      struct mq_attr attributes = {
          .mq_flags = 0, .mq_maxmsg = MAX_MSGS, .mq_msgsize = message_size, .mq_curmsgs = 0};

      common::util::assert_other(
          _queue = mq_open(
              queue_name.c_str(), O_CREAT | O_EXCL | O_NONBLOCK | O_RDWR, S_IRUSR | S_IWUSR,
              &attributes
          ),
          -1
      );

    } else {

      common::util::assert_other(_queue = mq_open(queue_name.c_str(), O_RDWR), -1);

      struct mq_attr attributes{};
      mq_getattr(_queue, &attributes);

      _msg_size = attributes.mq_msgsize;
    }

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

  void POSIXMQChannel::send(Message & msg, std::initializer_list<Buffer<char>> data)
  {
    size_t len = 0;
    for(auto buf : data) {
      len += buf.len;
    }

    msg.total_length(len);

    _send(msg.bytes(), msg.BUF_SIZE);
    for(auto buf : data) {
      _send(buf.val, buf.len);
    }
  }

  void POSIXMQChannel::_send(const int8_t* data, size_t len) const
  {
    // NOLINTNEXTLINE
    _send(reinterpret_cast<const char*>(data), len);
  }

  void POSIXMQChannel::_send(const char* data, size_t len) const
  {
    for(size_t pos = 0; pos < len; pos += _msg_size) {

      auto size = (len - pos < _msg_size) ? len - pos : _msg_size;
      mq_send(_queue, data + pos, size, 1);

    }
  }

  std::tuple<Message, Buffer<char>> POSIXMQChannel::receive()
  {
    Message msg;
    _recv(msg.data.data(), msg.BUF_SIZE);

    spdlog::error(msg.total_length());

    auto buf = _buffers.retrieve_buffer(msg.total_length());
    _recv(buf.val, buf.len);
    spdlog::error(msg.total_length());

    return std::make_tuple(msg, buf);
  }

  void POSIXMQChannel::_recv(int8_t* data, size_t len) const
  {
    // NOLINTNEXTLINE
    _recv(reinterpret_cast<char*>(data), len);
  }

  void POSIXMQChannel::_recv(char* data, size_t len) const
  {
    for(size_t pos = 0; pos < len; pos += _msg_size) {

      auto size = (len - pos < _msg_size) ? len - pos : _msg_size;
      size_t len = mq_receive(_queue, data + pos, size, nullptr);

    }
  }

} // namespace praas::process::ipc
