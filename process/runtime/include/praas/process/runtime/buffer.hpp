#ifndef PRAAS_PROCESS_RUNTIME_BUFFER_HPP
#define PRAAS_PROCESS_RUNTIME_BUFFER_HPP

#include <cstddef>

#include <boost/interprocess/streams/bufferstream.hpp>
#include <boost/iostreams/stream.hpp>
#include <cereal/archives/binary.hpp>

namespace praas::process::runtime {

  struct Buffer {

    std::byte* ptr{};

    size_t len{};

    size_t size{};

    // We cannot return char* directly - the string might not be NULL-terminated
    std::string_view str() const
    {
      return std::string_view(reinterpret_cast<char*>(ptr), len);
    }

    template <typename Obj>
    void deserialize(Obj& obj)
    {
      // Streams do not accept std::byte argument but char*
      boost::iostreams::stream<boost::iostreams::array_source> stream(
          reinterpret_cast<char*>(ptr), size
      );
      cereal::BinaryInputArchive archive_in{stream};
      obj.load(archive_in);
    }

    template <typename Obj>
    size_t serialize(const Obj& obj)
    {
      try {
        boost::interprocess::bufferstream out_stream(reinterpret_cast<char*>(ptr), size);
        cereal::BinaryOutputArchive archive_out{out_stream};
        archive_out(obj);

        len = out_stream.tellp();
      } catch (cereal::Exception& e) {
        std::cerr << "Serialization failed: " << e.what() << std::endl;
      }

      return len;
    }
  };

} // namespace praas::process::runtime

#endif
