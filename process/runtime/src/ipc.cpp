#include <praas/process/runtime/ipc/ipc.hpp>

#include <praas/common/exceptions.hpp>
#include <praas/common/util.hpp>

#include <thread>

#include <spdlog/spdlog.h>
#include <sys/signal.h>

namespace praas::process::runtime::ipc {

  IPCMode deserialize(std::string mode)
  {
    if (mode == "posix_mq") {
      return IPCMode::POSIX_MQ;
    } else {
      return IPCMode::NONE;
    }
  }

  IPCChannel::~IPCChannel() {}

  POSIXMQChannel::POSIXMQChannel(
      std::string queue_name, IPCDirection direction, bool create, int message_size
  )
      : _created(create), _name(queue_name), _msg_size(message_size),
        _buffers(BUFFER_ELEMS, BUFFER_SIZE)
  {

    int mq_direction = direction == IPCDirection::WRITE ? O_WRONLY : O_RDONLY;

    if (create) {

      struct mq_attr attributes {};
      attributes.mq_maxmsg = MAX_MSGS;
      attributes.mq_msgsize = message_size;

      _queue = mq_open(
          queue_name.c_str(), O_CREAT | O_EXCL | O_NONBLOCK | mq_direction, S_IRUSR | S_IWUSR,
          &attributes
      );
      if (_queue == -1 && errno == EEXIST) {

        // Attempt remove - unless it is used by another process
        mq_unlink(queue_name.c_str());

        common::util::assert_other(
            _queue = mq_open(
                queue_name.c_str(), O_CREAT | O_EXCL | O_NONBLOCK | mq_direction, S_IRUSR | S_IWUSR,
                &attributes
            ),
            -1
        );
      } else {
        common::util::assert_other(_queue, -1);
      }

    } else {

      common::util::assert_other(_queue = mq_open(queue_name.c_str(), mq_direction), -1);

      struct mq_attr attributes {};
      mq_getattr(_queue, &attributes);

      _msg_size = attributes.mq_msgsize;
    }

    _msg_buffer.reset(new int8_t[_msg_size]);

    SPDLOG_DEBUG("Opened message queue {}", _name);
  }

  POSIXMQChannel::~POSIXMQChannel()
  {
    shutdown();
  }

  void POSIXMQChannel::shutdown()
  {
    if(_queue == -1) {
      return;
    }

    if (_created) {
      common::util::assert_other(mq_close(_queue), -1);
      common::util::assert_other(mq_unlink(_name.c_str()), -1);

      SPDLOG_DEBUG("Closed message queue {}", _name);
    } else {
      common::util::assert_other(mq_close(_queue), -1);
    }
    _queue = -1;
  }

  int POSIXMQChannel::fd() const
  {
    // "On Linux, a message queue descriptor is actually a file
    // descriptor.  (POSIX does not require such an implementation.)
    return _queue;
  }

  void POSIXMQChannel::send(Message& msg)
  {
    msg.total_length(0);

    _send(msg.bytes(), msg.BUF_SIZE);
  }

  void POSIXMQChannel::send(Message& msg, BufferAccessor<char> buf)
  {
    SPDLOG_DEBUG("Sending message, buffer length {}", buf.len);
    msg.total_length(buf.len);

    _send(msg.bytes(), msg.BUF_SIZE);
    if (buf.len > 0)
      _send(buf.data(), buf.len);
  }

  void POSIXMQChannel::send(Message& msg, BufferAccessor<std::byte> buf)
  {
    SPDLOG_DEBUG("Sending message, buffer length {}", buf.len);
    msg.total_length(buf.len);

    _send(msg.bytes(), msg.BUF_SIZE);
    if (buf.len > 0) {
      _send(reinterpret_cast<char*>(buf.data()), buf.len);
    }
  }

  void POSIXMQChannel::send(Message& msg, const std::vector<Buffer<char>>& data)
  {
    size_t len = 0;
    for (const auto & buf : data) {
      len += buf.len;
    }

    msg.total_length(len);

    _send(msg.bytes(), msg.BUF_SIZE);
    for (const auto & buf : data) {
      if (buf.len > 0) {
        _send(buf.data(), buf.len);
      }
    }
  }

  void POSIXMQChannel::_send(const int8_t* data, int len) const
  {
    // NOLINTNEXTLINE
    _send(reinterpret_cast<const char*>(data), len);
  }

  void POSIXMQChannel::_send(const char* data, int len) const
  {

    for (int pos = 0; pos < len;) {

      auto size = (len - pos < _msg_size) ? len - pos : _msg_size;
      int ret = mq_send(_queue, data + pos, size, 1);
      SPDLOG_DEBUG("Sending status {}, {} bytes, at pos {}, out of {} bytes to sent, errno {}", ret, size, pos, len, errno);

      if (ret == -1) {

        if (errno == EAGAIN) {
          // FIXME: add to epoll as oneshot to be woken up as ready
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

  bool POSIXMQChannel::blocking_receive(Buffer<std::byte> & buf)
  {
    size_t read_data = _recv(_msg_buffer.get(), Message::BUF_SIZE);
    // We do not support sending partial message headers.
    if(read_data < Message::BUF_SIZE) {
      throw praas::common::NotImplementedError();
    }

    // FIXME: avoid a copy here?
    std::copy_n(_msg_buffer.get(), Message::BUF_SIZE, _msg.data.data());

    size_t data_to_read = _msg.total_length();
    if(buf.size < data_to_read) {
      buf.resize(data_to_read);
    }

    read_data = _recv(buf.data(), data_to_read);
    buf.len = read_data;
    SPDLOG_DEBUG("Read {} bytes out of queue {}", read_data, _name);
    return read_data >= data_to_read;
  }

  std::tuple<bool, Buffer<char>> POSIXMQChannel::receive()
  {
    // Did we read message header in the previous call?
    if (!_msg_read) {

      size_t read_data = _recv(_msg_buffer.get(), Message::BUF_SIZE);

      // We did not manage to read any data
      if(read_data == 0) {
        return std::make_tuple(false, Buffer<char>{});
      }

      // We do not support sending partial message headers.
      if(read_data < Message::BUF_SIZE) {
        throw praas::common::NotImplementedError();
      }

      // FIXME: avoid a copy here?
      std::copy_n(_msg_buffer.get(), Message::BUF_SIZE, _msg.data.data());

      _msg_read = true;
    }

    if (_msg.total_length() > 0) {

      // (1) We just read message, buffer not allocated.
      // (2) We read message in previous epoll, buffer not allocated.
      // (3) We read message in previous allocated, buffer allocated, data not read completely.
      if(_msg_payload.null()) {
        // Buffer cannot be smaller than message queue size
        size_t size = std::max(_msg.total_length(), _msg_size);
        _msg_payload = _buffers.retrieve_buffer(size);
        assert(!_msg_payload.null());
      }

      // Depending on the situation, we read different amount of data
      // (1) & (2) - start from 0 position, attempt to read everything
      // (3) - start when we left things last time, attempt to read the rest
      size_t data_to_read = _msg.total_length() - _msg_payload.len;
      size_t read_data = _recv(_msg_payload.data() + _msg_payload.len, data_to_read);
      _msg_payload.len += read_data;

      // We read everything
      if(read_data == data_to_read) {

        _msg_read = false;

        // Return buffer with payload, restart the buffer for future use.
        Buffer<char> buf = std::move(_msg_payload);
        _msg_payload = Buffer<char>{};

        return std::make_tuple(true, std::move(buf));

      } else {
        return std::make_tuple(false, Buffer<char>{});
      }
    } else {
      _msg_read = false;
      return std::make_tuple(true, Buffer<char>{});
    }

  }

  size_t POSIXMQChannel::_recv(std::byte * data, size_t len) const
  {
    // NOLINTNEXTLINE
    return _recv(reinterpret_cast<char*>(data), len);
  }

  size_t POSIXMQChannel::_recv(int8_t* data, size_t len) const
  {
    // NOLINTNEXTLINE
    return _recv(reinterpret_cast<char*>(data), len);
  }

  size_t POSIXMQChannel::_recv(char* data, size_t len) const
  {
    size_t pos = 0;
    for (; pos < len;) {

      long rcv_len = mq_receive(_queue, data + pos, _msg_size, nullptr);

      if (rcv_len == -1 && errno == EAGAIN) {
        break;
      } else if (rcv_len == -1) {
        throw praas::common::PraaSException{
            fmt::format("Failed receiving with error {}, strerror {}", errno, strerror(errno))};
      }
      pos += rcv_len;
    }

    return pos;
  }

} // namespace praas::process::runtime::ipc
