#ifndef PRAAS_FUNCTION_CONTEXT_HPP
#define PRAAS_FUNCTION_CONTEXT_HPP

#include <praas/function/buffer.hpp>
#include <praas/process/runtime/buffer.hpp>

#include <memory>
#include <string>
#include <vector>

namespace praas::process {
  struct Invoker;
} // namespace praas::process

namespace praas::function {

  struct Context {

    // FIXME: byte arguments
    void put(std::string name);
    void get(std::string name);
    // FIXME: state ops
    // FIXME: invocation ops

    // The pointer must stay valid until the end
    Buffer& get_output_buffer(size_t size = 0);

    void write_output(const std::byte* ptr, size_t len, size_t pos);

    void start_invocation(const std::string& invoc_id)
    {
      _invoc_id = invoc_id;
      _output.len = 0;
    }

    std::string invocation_id() const
    {
      return _invoc_id;
    }

  private:

    process::runtime::BufferAccessor<char> as_buffer() const;

    // Now we always allocate a buffer - but this can be a shared memory object in future.
    static constexpr int BUFFER_SIZE = 1024 * 1024 * 5;

    Context(process::Invoker& invoker):
      _invoker(invoker),
      _output(new std::byte[BUFFER_SIZE], BUFFER_SIZE, 0),
      _output_buf_view{_output.ptr.get(), _output.len, _output.size}
    {}

    process::Invoker& _invoker;

    process::runtime::Buffer<std::byte> _output;

    Buffer _output_buf_view;

    std::string _invoc_id;

    friend struct process::Invoker;
  };

} // namespace praas::function

#endif
