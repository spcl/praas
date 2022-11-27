#ifndef PRAAS_PROCESS_RUNTIME_IPC_IPC_HPP
#define PRAAS_PROCESS_RUNTIME_IPC_IPC_HPP

#include <praas/process/runtime/buffer.hpp>
#include <praas/process/runtime/ipc/messages.hpp>

#include <optional>
#include <string>
#include <tuple>

#include <mqueue.h>

namespace praas::process::runtime::ipc {

  enum IPCMode { POSIX_MQ, NONE };

  enum IPCDirection { WRITE, READ };

  IPCMode deserialize(std::string);

  struct IPCChannel {

    virtual ~IPCChannel() = 0;

    virtual int fd() const = 0;

    virtual void send(Message& msg, const std::vector<Buffer<char>>& data) = 0;

    virtual void send(Message& msg, Buffer<char> data) = 0;

    virtual std::tuple<Message, Buffer<char>> receive() = 0;
  };

  struct POSIXMQChannel : public IPCChannel {

    static constexpr int MAX_MSGS = 10;
    static constexpr int MAX_MSG_SIZE = 8 * 1024;

    static constexpr int BUFFER_ELEMS = 5;
    static constexpr int BUFFER_SIZE = 1 * 1024 * 1024;

    POSIXMQChannel(
        std::string queue_name, IPCDirection direction, bool create = false,
        int msg_size = MAX_MSG_SIZE
    );
    virtual ~POSIXMQChannel();

    std::string name() const
    {
      return _name;
    }

    int fd() const override;

    std::tuple<Message, Buffer<char>> receive() override;

    void send(Message& msg, const std::vector<Buffer<char>>& data) override;
    void send(Message& msg, Buffer<char> buf) override;

  private:
    mqd_t _queue;

    bool _created;

    std::string _name;

    int _msg_size;

    BufferQueue<char> _buffers;

    std::unique_ptr<int8_t[]> _msg_buffer;

    void _send(const char* data, int len) const;
    void _send(const int8_t* data, int len) const;
    void _recv(int8_t* data, int len) const;
    void _recv(char* data, int len) const;
  };

} // namespace praas::process::runtime::ipc

#endif
