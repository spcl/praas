#ifndef PRAAS_PROCESS_TESTS_INTEGRATION_TEST_HPP
#define PRAAS_PROCESS_TESTS_INTEGRATION_TEST_HPP

#include <cereal/types/string.hpp>
#include <cereal/archives/binary.hpp>

struct Input
{
  int arg1;
  int arg2;

  void save(cereal::BinaryOutputArchive& archive) const
  {
    archive(arg1);
    archive(arg2);
  }

  void load(cereal::BinaryInputArchive& archive)
  {
    archive(arg1);
    archive(arg2);
  }
};

struct Output
{
  int result;

  void save(cereal::BinaryOutputArchive& archive) const
  {
    archive(result);
  }

  void load(cereal::BinaryInputArchive& archive)
  {
    archive(result);
  }
};

struct Message
{
  std::string message;
  int some_data;

  void save(cereal::BinaryOutputArchive& archive) const
  {
    archive(message);
    archive(some_data);
  }

  void load(cereal::BinaryInputArchive& archive)
  {
    archive(message);
    archive(some_data);
  }
};

#endif
