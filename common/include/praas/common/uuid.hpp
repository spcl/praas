#ifndef PRAAS_COMMON_UUID_HPP
#define PRAAS_COMMON_UUID_HPP

#include <span>

#include <uuid.h>

namespace praas::common {

  class UUID {
  public:
    static constexpr int UUID_LEN = 16;

    UUID() : _generator{_rd()}, _uuid_generator{_generator} {}

    void generate(std::span<std::byte, UUID_LEN> dest)
    {
      uuids::uuid uuid = _uuid_generator();
      std::copy_n(uuid.as_bytes().begin(), UUID_LEN, dest.begin());
    }

    uuids::uuid generate()
    {
      return _uuid_generator();
    }

    static std::string str(std::span<const std::byte, UUID_LEN> dest)
    {
      // they expose std::byte, but constructors accept uint8_- why?
      // https://github.com/mariusbancila/stduuid/issues/70
      const uint8_t* begin = reinterpret_cast<const uint8_t*>(dest.data());
      const uint8_t* end = reinterpret_cast<const uint8_t*>(dest.data() + dest.size_bytes());
      return uuids::to_string(uuids::uuid(begin, end));
    }

  private:
    std::random_device _rd;
    std::mt19937 _generator;
    uuids::uuid_random_generator _uuid_generator;
  };

} // namespace praas::common

#endif
