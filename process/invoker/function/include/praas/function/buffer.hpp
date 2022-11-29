#ifndef PRAAS_FUNCTION_BUFFER_HPP
#define PRAAS_FUNCTION_BUFFER_HPP

#include <cstddef>>

#include <boost/iostreams/stream.hpp>
#include <boost/interprocess/streams/bufferstream.hpp>
#include <cereal/archives/binary.hpp>

namespace praas::function {

  struct Buffer {

    std::byte* ptr{};

    size_t len{};

    size_t size{};

    template<typename Obj>
    void deserialize(Obj & obj)
    {
      // Streams do not accept std::byte argument but char*
      boost::iostreams::stream<boost::iostreams::array_source> stream(
        reinterpret_cast<char*>(ptr), size
      );
      cereal::BinaryInputArchive archive_in{stream};
      obj.load(archive_in);
    }

    template<typename Obj>
    size_t serialize(const Obj & obj)
    {
      try {
        boost::interprocess::bufferstream out_stream(reinterpret_cast<char*>(ptr), size);
        cereal::BinaryOutputArchive archive_out{out_stream};
        archive_out(obj);

        len = out_stream.tellp();
      } catch(cereal::Exception & e) {
        std::cerr << "Serialization failed: " << e.what() << std::endl;
      }

      return len;
    }
  };

}

#endif
