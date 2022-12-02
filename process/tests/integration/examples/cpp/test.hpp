#ifndef PRAAS_PROCESS_TESTS_INTEGRATION_TEST_HPP
#define PRAAS_PROCESS_TESTS_INTEGRATION_TEST_HPP

#include <cereal/types/string.hpp>
#include <cereal/archives/binary.hpp>

struct Input
{
  int arg1;
  int arg2;

  template<typename Ar>
  void save(Ar & archive) const
  {
    archive(CEREAL_NVP(arg1));
    archive(CEREAL_NVP(arg2));
  }

  template<typename Ar>
  void load(Ar & archive)
  {
    archive(CEREAL_NVP(arg1));
    archive(CEREAL_NVP(arg2));
  }
};

struct Output
{
  int result{};

  template<typename Ar>
  void save(Ar & archive) const
  {
    archive(CEREAL_NVP(result));
  }

  template<typename Ar>
  void load(Ar & archive)
  {
    archive(CEREAL_NVP(result));
  }
};

struct Message
{
  std::string message;
  int some_data;

  void save(cereal::BinaryOutputArchive& archive) const
  {
    archive(CEREAL_NVP(message));
    archive(CEREAL_NVP(some_data));
  }

  void load(cereal::BinaryInputArchive& archive)
  {
    archive(CEREAL_NVP(message));
    archive(CEREAL_NVP(some_data));
  }
};

struct InputMsgKey
{
  std::string message_key;

  template<typename Ar>
  void save(Ar & archive) const
  {
    archive(CEREAL_NVP(message_key));
  }

  template<typename Ar>
  void load(Ar & archive)
  {
    archive(CEREAL_NVP(message_key));
  }
};

#endif
