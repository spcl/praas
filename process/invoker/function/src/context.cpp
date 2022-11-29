#include <praas/function/context.hpp>

#include <praas/process/invoker.hpp>
#include <praas/process/runtime/buffer.hpp>
#include <praas/process/runtime/ipc/messages.hpp>
#include "praas/common/exceptions.hpp"

namespace praas::function {

  Buffer& Context::get_output_buffer(size_t size)
  {
    if (size != 0 && size > _output.size) {
      _output.resize(size);
      _output_buf_view = Buffer{_output.ptr.get(), _output.len, _output.size};
    }

    return _output_buf_view;
  }

  Buffer Context::get_buffer(size_t size)
  {
    _user_buffers.emplace_back(new std::byte[size], 0, size);
    return Buffer{
        _user_buffers.back().ptr.get(), _user_buffers.back().size, _user_buffers.back().len};
  }

  void Context::write_output(const std::byte* ptr, size_t len, size_t pos)
  {
    memcpy(_output.data() + pos, ptr, len);
    _output_buf_view.len = std::max(_output_buf_view.len, pos + len);
  }

  process::runtime::BufferAccessor<char> Context::as_buffer() const
  {
    auto acc = _output.accessor<char>();
    // Keep track of length changes that were done by users.
    acc.len = _output_buf_view.len;
    return acc;
  }

  void
  Context::put(std::string_view destination, std::string_view msg_key, std::byte* ptr, size_t size)
  {
    process::runtime::ipc::PutRequest req;
    req.process_id(destination);
    req.name(msg_key);
    req.data_len(size);

    // User data - for shm, it might have to be copied!
    _invoker.put(req, process::runtime::BufferAccessor<std::byte>{ptr, size});
  }

  void Context::put(std::string_view destination, std::string_view msg_key, Buffer buf)
  {
    process::runtime::ipc::PutRequest req;
    req.process_id(destination);
    req.name(msg_key);
    req.data_len(buf.size);

    // find the buffer
    for (auto& user_buf : _user_buffers) {
      if (user_buf.data() == buf.ptr) {
        _invoker.put(req, user_buf);
        return;
      }
    }
    // Buffer not found
    throw common::PraaSException{"Submitted put request with a non-existing buffer!"};
  }

  Buffer Context::get(std::string_view source, std::string_view msg_key)
  {
    process::runtime::ipc::GetRequest req;
    req.process_id(source);
    req.name(msg_key);

    auto [request, data] = _invoker.get(req);

    if(req.data_len() < 0) {
      throw common::FunctionGetFailure(fmt::format(
          "Get failed!"
      ));
    }

    if (req.process_id() != SELF && req.process_id() != ANY && req.process_id() != request.process_id()) {
      throw common::FunctionGetFailure(fmt::format(
          "Received incorrect get result - incorrect process id {}", request.process_id()
      ));
    }

    if (req.name() != request.name()) {
      throw common::FunctionGetFailure(
          fmt::format("Received incorrect get result - incorrect name id {}", request.name())
      );
    }

    _user_buffers.push_back(std::move(data));
    auto& buf = _user_buffers.back();

    return Buffer{buf.ptr.get(), buf.len, buf.size};
  }

}; // namespace praas::function
