#ifndef PRAAS_PROCESS_RUNTIME_BUFFER_HPP
#define PRAAS_PROCESS_RUNTIME_BUFFER_HPP

#include <cstdint>
#include <queue>
#include <string>

#include <spdlog/spdlog.h>

namespace praas::process::runtime {

  template <typename T>
  struct BufferAccessor {
    T* ptr{};
    size_t len{};

    BufferAccessor() = default;
    BufferAccessor(T* ptr, size_t len) : ptr(ptr), len(len) {}

    T* data() const
    {
      return ptr;
    }

    bool empty() const
    {
      return len == 0;
    }

    bool null() const
    {
      return ptr == 0;
    }

  };

  template <typename T>
  struct Buffer {
    std::unique_ptr<T[]> ptr{};
    size_t size{};
    size_t len{};

    Buffer() = default;
    Buffer(T* ptr, size_t size, size_t len = 0) : ptr(ptr), size(size), len(len) {}

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer &&) noexcept = default;
    Buffer& operator=(Buffer &&) noexcept = default;

    ~Buffer() = default;

    T* data() const
    {
      return ptr.get();
    }

    bool empty() const
    {
      return len == 0;
    }

    bool null() const
    {
      return ptr == 0;
    }

    void resize(size_t size)
    {
      ptr.reset(new T[size]);
      this->size = size;
      this->len = 0;
    }

    operator BufferAccessor<T>() const
    {
      return BufferAccessor<T>(ptr, size);
    }

    template<typename U>
    BufferAccessor<U> accessor() const
    {
      return BufferAccessor<U>(reinterpret_cast<U*>(ptr.get()), size);
    }
  };

  template <typename T>
  struct BufferQueue {

    typedef Buffer<T> val_t;

    std::queue<val_t> _buffers;

    BufferQueue() = default;

    BufferQueue(size_t elements, size_t elem_size)
    {
      for (size_t i = 0; i < elements; ++i) {
        _buffers.push(val_t{new T[elem_size], elem_size});
      }
    }

    ~BufferQueue() = default;

    Buffer<T> retrieve_buffer(size_t size)
    {
      if (_buffers.empty()) {
        return _allocate_buffer(size);
      }

      Buffer<T> buf = std::move(_buffers.front());
      _buffers.pop();
      if (_buffers.size() < size) {
        buf.resize(size);
      }

      return buf;
    }

    void return_buffer(Buffer<T>&& buf)
    {
      buf.len = 0;
      _buffers.push(std::move(buf));
    }

    Buffer<T> _allocate_buffer(size_t size)
    {
      return Buffer<T>{new T[size], size, 0};
    }
  };

} // namespace praas::process::runtime

#endif
