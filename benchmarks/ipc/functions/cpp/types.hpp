
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/tuple.hpp>
#include <cereal/archives/binary.hpp>

struct Invocations
{
  bool sender;
  std::string bucket;
  int repetitions;
  std::vector<int> sizes;
  std::string redis_hostname;

  template<typename Ar>
  void load(Ar & archive)
  {
    archive(CEREAL_NVP(repetitions));
    archive(CEREAL_NVP(sizes));
    archive(CEREAL_NVP(sender));
    archive(CEREAL_NVP(bucket));
    archive(CEREAL_NVP(redis_hostname));
  }

  template<typename Ar>
  void save(Ar & archive) const
  {
    archive(CEREAL_NVP(repetitions));
    archive(CEREAL_NVP(sizes));
    archive(CEREAL_NVP(sender));
    archive(CEREAL_NVP(bucket));
    archive(CEREAL_NVP(redis_hostname));
  }
};

struct Results
{
  std::vector< std::vector<std::tuple<long, long>> > measurements;
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
