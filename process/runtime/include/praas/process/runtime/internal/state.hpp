#ifndef PRAAS_PROCESS_RUNTIME_STATE_HPP
#define PRAAS_PROCESS_RUNTIME_STATE_HPP

#include <string>
#include <vector>

#include <boost/interprocess/streams/bufferstream.hpp>
#include <boost/iostreams/stream.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/tuple.hpp>
#include <cereal/types/vector.hpp>

namespace praas::process::runtime::internal {

  struct StateKeys {

    std::vector<std::tuple<std::string, double>> keys;

    template <typename Ar>
    void save(Ar& archive) const
    {
      archive(CEREAL_NVP(keys));
    }

    template <typename Ar>
    void load(Ar& archive)
    {
      archive(CEREAL_NVP(keys));
    }

    static std::string serialize(const std::vector<std::tuple<std::string, double>>& keys)
    {
      // FIXME: avoid copy here
      StateKeys out{keys};
      // FIXME: buffers
      std::stringstream str;
      cereal::BinaryOutputArchive archive_in{str};
      out.save(archive_in);
      return str.str();
    }

    static StateKeys deserialize(const char* buffer, size_t size)
    {
      StateKeys keys;
      boost::iostreams::stream<boost::iostreams::array_source> stream(buffer, size);
      cereal::BinaryInputArchive archive_in{stream};
      keys.load(archive_in);
      return keys;
    }
  };

} // namespace praas::process::runtime::internal

#endif
