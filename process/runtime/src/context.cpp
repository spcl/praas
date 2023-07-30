#include <praas/process/runtime/context.hpp>

#include <praas/common/exceptions.hpp>
#include <praas/process/runtime/buffer.hpp>
#include <praas/process/runtime/internal/invoker.hpp>
#include <praas/process/runtime/internal/ipc/messages.hpp>

namespace praas::process::runtime {

  Buffer& Context::get_output_buffer(size_t size)
  {
    if (size != 0 && size > _output.size) {
      _output.resize(size);
      _output_buf_view = Buffer{_output.ptr.get(), _output.len, _output.size};
    }

    return _output_buf_view;
  }

  void Context::set_output_buffer(Buffer buf)
  {
    _output_buf_view = buf;
  }

  Buffer Context::get_buffer(size_t size)
  {
    _user_buffers.emplace_back(new std::byte[size], size, 0);
    return Buffer{
        _user_buffers.back().ptr.get(), _user_buffers.back().len, _user_buffers.back().size};
  }

  void Context::write_output(const std::byte* ptr, size_t len, size_t pos)
  {
    memcpy(_output.data() + pos, ptr, len);
    _output_buf_view.len = std::max(_output_buf_view.len, pos + len);
  }

  internal::BufferAccessor<const char> Context::as_buffer() const
  {
    // Has the output buffer been overriden?
    if (_output_buf_view.ptr != _output.ptr.get()) {
      return internal::BufferAccessor<const char>{
          reinterpret_cast<const char*>(_output_buf_view.ptr), _output_buf_view.len};
    } else {
      auto acc = _output.accessor<const char>();
      // Keep track of length changes that were done by users.
      acc.len = _output_buf_view.len;
      return acc;
    }
  }

  void Context::state(std::string_view msg_key, Buffer buf)
  {
    internal::ipc::PutRequest req;
    req.name(msg_key);
    req.data_len(buf.len);
    req.state(true);

    // find the buffer
    for (auto& user_buf : _user_buffers) {
      if (user_buf.data() == buf.ptr) {
        user_buf.len = buf.len;
        _invoker.put(req, user_buf);
        return;
      }
    }
    // Buffer not found
    throw common::PraaSException{"Submitted put request with a non-existing buffer!"};
  }

  Buffer Context::state(std::string_view msg_key)
  {
    internal::ipc::GetRequest req;
    req.name(msg_key);
    req.state(true);

    auto [request, data] = _invoker.get(req);

    if (req.data_len() < 0) {
      throw common::FunctionGetFailure(fmt::format("Get failed!"));
    }

    if (req.name() != request.name()) {
      throw common::FunctionGetFailure(
          fmt::format("Received incorrect get result - incorrect name id {}", request.name())
      );
    }

    if (data.len != 0) {
      _user_buffers.push_back(std::move(data));
      auto& buf = _user_buffers.back();
      return Buffer{buf.ptr.get(), buf.len, buf.size};
    } else {
      return Buffer{};
    }
  }

  void
  Context::put(std::string_view destination, std::string_view msg_key, std::byte* ptr, size_t size)
  {
    internal::ipc::PutRequest req;
    req.process_id(destination);
    req.name(msg_key);
    req.data_len(size);

    // User data - for shm, it might have to be copied!
    _invoker.put(req, internal::BufferAccessor<std::byte>{ptr, size});
  }

  void Context::put(std::string_view destination, std::string_view msg_key, Buffer buf)
  {
    internal::ipc::PutRequest req;
    req.process_id(destination);
    req.name(msg_key);
    req.data_len(buf.len);

    // find the buffer
    for (auto& user_buf : _user_buffers) {
      if (user_buf.data() == buf.ptr) {
        user_buf.len = buf.len;
        _invoker.put(req, user_buf);
        return;
      }
    }
    // Buffer not found
    throw common::PraaSException{"Submitted put request with a non-existing buffer!"};
  }

  Buffer Context::get(std::string_view source, std::string_view msg_key)
  {
    internal::ipc::GetRequest req;
    req.process_id(source);
    req.name(msg_key);

    auto [request, data] = _invoker.get(req);

    if (req.data_len() < 0) {
      throw common::FunctionGetFailure(fmt::format("Get failed!"));
    }

    if (req.process_id() != SELF && req.process_id() != ANY &&
        req.process_id() != request.process_id()) {
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

  const std::vector<std::string>& Context::active_processes() const
  {
    return _invoker.application().active_processes;
  }

  const std::vector<std::string>& Context::swapped_processes() const
  {
    return _invoker.application().swapped_processes;
  }

  InvocationResult Context::invoke(
      std::string_view process_id, std::string_view function_name, std::string_view invocation_id,
      Buffer input
  )
  {
    internal::ipc::InvocationRequest req;
    req.process_id(process_id);
    req.function_name(function_name);
    req.invocation_id(invocation_id);
    req.buffers(input.len);

    // find the buffer
    bool submitted = false;
    for (auto& user_buf : _user_buffers) {
      if (user_buf.data() == input.ptr) {
        user_buf.len = input.len;
        _invoker.put(req, user_buf);
        submitted = true;
      }
    }
    // Buffer not found
    if (!submitted)
      throw common::PraaSException{"Submitted invoke request with a non-existing buffer!"};

    auto [result, data] = _invoker.get<internal::ipc::InvocationResultParsed>();

    if (req.invocation_id() != result.invocation_id()) {
      throw common::FunctionGetFailure(fmt::format(
          "Received incorrect invocation result - incorrect invocation id {}",
          result.invocation_id()
      ));
    }

    _user_buffers.push_back(std::move(data));
    auto& buf = _user_buffers.back();

    return InvocationResult{
        std::string{invocation_id}, std::string{function_name}, result.return_code(),
        Buffer{buf.ptr.get(), buf.len, buf.size}};
  }

}; // namespace praas::process::runtime
