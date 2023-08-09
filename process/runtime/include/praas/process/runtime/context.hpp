#ifndef PRAAS_PROCESS_RUNTIME_CONTEXT_HPP
#define PRAAS_PROCESS_RUNTIME_CONTEXT_HPP

#include <praas/process/runtime/buffer.hpp>
#include <praas/process/runtime/internal/buffer.hpp>
#include <praas/process/runtime/invocation.hpp>

#include <memory>
#include <string>
#include <vector>

namespace praas::process::runtime::internal {
  struct Invoker;
} // namespace praas::process::runtime::internal

namespace praas::process::runtime {

  struct Context {

    static constexpr std::string_view SELF = "SELF";

    static constexpr std::string_view ANY = "ANY";

    // FIXME: byte arguments
    void put(std::string_view destination, std::string_view msg_key, std::byte* ptr, size_t size);

    // Prefered way - if we use shm, we want to write directly to a buffer and just transport
    // the location of the message.
    void put(std::string_view destination, std::string_view msg_key, Buffer buf);

    std::vector<std::string> state_keys();

    void state(std::string_view msg_key, Buffer buf);

    Buffer state(std::string_view msg_key);

    void state(std::string_view msg_key, std::string_view data);

    // Non-owning!
    Buffer get(std::string_view source, std::string_view msg_key);

    InvocationResult invoke(
        std::string_view process_id, std::string_view function, std::string_view invocation_id,
        Buffer input
    );

    // FIXME: state ops
    // FIXME: invocation ops

    std::string invocation_id() const
    {
      return _invoc_id;
    }

    std::string process_id() const
    {
      return _process_id;
    }

    // The pointer must stay valid until the end
    Buffer& get_output_buffer(size_t size = 0);

    void set_output_buffer(Buffer buf);

    Buffer get_buffer(size_t size);

    void write_output(const std::byte* ptr, size_t len, size_t pos);

    void start_invocation(const std::string& invoc_id)
    {
      _invoc_id = invoc_id;
      _output.len = 0;
      _output_buf_view.len = 0;
    }

    void end_invocation()
    {
      _user_buffers.clear();
    }

    const std::vector<std::string>& active_processes() const;

    const std::vector<std::string>& swapped_processes() const;

    // make private
    internal::BufferAccessor<const char> as_buffer() const;

  private:
    // Now we always allocate a buffer - but this can be a shared memory object in future.
    static constexpr int BUFFER_SIZE = 1024 * 1024 * 5;

    Context(std::string process_id, internal::Invoker& invoker)
        : _invoker(invoker), _output(new std::byte[BUFFER_SIZE], BUFFER_SIZE, 0),
          _output_buf_view{_output.ptr.get(), _output.len, _output.size},
          _process_id(std::move(process_id))
    {
    }

    internal::Invoker& _invoker;

    internal::Buffer<std::byte> _output;

    std::vector<internal::Buffer<std::byte>> _user_buffers;

    Buffer _output_buf_view;

    std::string _invoc_id;

    std::string _process_id;

    friend struct internal::Invoker;
  };

} // namespace praas::process::runtime

#endif
