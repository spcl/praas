
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/tuple.hpp>
#include <cereal/archives/binary.hpp>

struct Invocations
{
  std::string bucket;
  std::string redis_hostname;
  std::vector<int> sizes;
  int repetitions;
  int threads;

  template<typename Ar>
  void load(Ar & archive)
  {
    archive(CEREAL_NVP(sizes));
    archive(CEREAL_NVP(threads));
    archive(CEREAL_NVP(repetitions));
    archive(CEREAL_NVP(bucket));
    archive(CEREAL_NVP(redis_hostname));
  }

  template<typename Ar>
  void save(Ar & archive) const
  {
    archive(CEREAL_NVP(sizes));
    archive(CEREAL_NVP(threads));
    archive(CEREAL_NVP(repetitions));
    archive(CEREAL_NVP(bucket));
    archive(CEREAL_NVP(redis_hostname));
  }
};

struct Results
{
  std::vector< std::vector<long> > measurements;
  template<typename Ar>
  void load(Ar & archive)
  {
    archive(CEREAL_NVP(measurements));
  }

  template<typename Ar>
  void save(Ar & archive) const
  {
    archive(CEREAL_NVP(measurements));
  }
};
