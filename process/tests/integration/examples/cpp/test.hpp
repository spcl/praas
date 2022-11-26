#ifndef PRAAS_PROCESS_TESTS_INTEGRATION_TEST_HPP
#define PRAAS_PROCESS_TESTS_INTEGRATION_TEST_HPP

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

  void save(cereal::BinaryOutputArchive& archive)
  {
    archive(result);
  }
};

#endif
