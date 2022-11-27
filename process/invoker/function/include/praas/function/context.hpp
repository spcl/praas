#ifndef PRAAS_FUNCTION_CONTEXT_HPP
#define PRAAS_FUNCTION_CONTEXT_HPP

#include <praas/process/runtime/buffer.hpp>

#include <memory>
#include <string>
#include <vector>

namespace praas::process {
  struct Invoker;
} // namespace praas::process

namespace praas::function {

  struct Buffer {
    std::byte* ptr;

    size_t size;
  };

  struct Context {

    // FIXME: byte arguments
    void put(std::string name);
    void get(std::string name);

    // The pointer must stay valid until the end
    Buffer get_output_buffer(size_t size = 0);

    void set_output_len(size_t len);

    size_t get_output_len();

    void write_output(const std::byte* ptr, size_t len, size_t pos);

    void start_invocation(const std::string& invoc_id)
    {
      _invoc_id = invoc_id;
      _buffer_len = 0;
    }

    std::string invocation_id() const
    {
      return _invoc_id;
    }

  private:

    process::runtime::Buffer<char> as_buffer() const;

    // Now we always allocate a buffer - but this can be a shared memory object in future.
    static constexpr int BUFFER_SIZE = 1024 * 1024 * 5;

    Context(process::Invoker& invoker):
      _invoker(invoker),
      _buffer(new std::byte[BUFFER_SIZE]),
      _buffer_size(BUFFER_SIZE)
    {}

    process::Invoker& _invoker;

    std::string _invoc_id;

    std::unique_ptr<std::byte[]> _buffer;

    size_t _buffer_len{};

    size_t _buffer_size;

    friend struct process::Invoker;
  };

} // namespace praas::function

#endif
