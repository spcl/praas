#include <praas/function/context.hpp>

#include <praas/process/invoker.hpp>

namespace praas::function {

  Buffer& Context::get_output_buffer(size_t size)
  {
    if(size != 0 && size > _output.size) {
      _output.resize(size);
      _output_buf_view = Buffer{_output.ptr.get(), _output.len, _output.size};
    }

    return _output_buf_view;
  }

  void Context::write_output(const std::byte* ptr, size_t len, size_t pos)
  {
    memcpy(_output.data() + pos, ptr, len);
    _output_buf_view.len = std::max(_output_buf_view.len, pos + len);
  }

  process::runtime::BufferAccessor<char> Context::as_buffer() const
  {
    auto acc =_output.accessor<char>();
    // Keep track of length changes that were done by users.
    acc.len = _output_buf_view.len;
    return acc;
  }

};
