#include <praas/function/context.hpp>

#include <praas/process/invoker.hpp>

namespace praas::function {

  Buffer Context::get_output_buffer()
  {
    return {_buffer.get(), _buffer_size};
  }

  void Context::set_output_len(size_t len)
  {
    _buffer_len = len;
  }

  size_t Context::get_output_len()
  {
    return _buffer_len;
  }

  void Context::write_output(const std::byte* ptr, size_t len, size_t pos)
  {
    memcpy(_buffer.get() + pos, ptr, len);
    _buffer_len = std::max(_buffer_len, pos + len);
  }

  process::runtime::Buffer<char> Context::as_buffer() const
  {
    return {reinterpret_cast<char*>(_buffer.get()), _buffer_size, _buffer_len};
  }

};
