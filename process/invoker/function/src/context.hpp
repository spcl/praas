#include <praas/function/context.hpp>

#include <praas/process/invoker.hpp>

namespace praas::function {

  Buffer Context::output_buffer(size_t len)
  {

    _payloads.emplace_back(payload, payload_size);
  }

  void Context::write_output(const std::byte* ptr, size_t len, size_t pos)
  {
    memcpy(_buffer.data() + pos, ptr, len);
  }

};
