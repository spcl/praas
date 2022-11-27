#ifndef PRAAS_PROCESS_RUNTIME_BUFFER_HPP
#define PRAAS_PROCESS_RUNTIME_BUFFER_HPP

#include <cstdint>
#include <queue>
#include <string>

#include <spdlog/spdlog.h>

namespace praas::process::runtime {

  template <typename T>
  struct Buffer {
    T* val{};
    size_t size{};
    size_t len{};

    Buffer() = default;
    Buffer(T* val, size_t size, size_t len = 0) : val(val), size(size), len(len) {}

    bool empty() const
    {
      return len == 0;
    }

    bool null() const
    {
      return val == 0;
    }
  };

  template <typename T>
  struct BufferQueue {

    typedef Buffer<T> val_t;

    std::queue<val_t> _buffers;
    size_t _elements;

    BufferQueue(size_t elements, size_t elem_size) : _elements(elements)
    {
      for (size_t i = 0; i < _elements; ++i)
        _buffers.push(val_t{new T[elem_size], elem_size});
    }

    ~BufferQueue()
    {
      while (!_buffers.empty()) {
        delete[] _buffers.front().val;
        _buffers.pop();
      }
    }

    Buffer<T> retrieve_buffer(size_t size)
    {
      if (_buffers.empty())
        return {nullptr, 0};

      Buffer<T> buf = _buffers.front();
      if (_buffers.size() < size) {
        delete[] buf.val;
        buf.val = new T[size];
        buf.size = size;
      }
      _buffers.pop();

      buf.len = 0;

      return buf;
    }

    void return_buffer(Buffer<T>&& buf)
    {
      _buffers.push(buf);
    }

    void return_buffer(const Buffer<T>& buf)
    {
      _buffers.push(buf);
    }
  };

} // namespace praas::process::runtime

#endif
