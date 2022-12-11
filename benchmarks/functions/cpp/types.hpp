
#include <cereal/types/vector.hpp>
#include <cereal/archives/binary.hpp>

struct Invocations
{
  int repetitions;
  std::vector<int> sizes;

  template<typename Ar>
  void load(Ar & archive)
  {
    archive(CEREAL_NVP(repetitions));
    archive(CEREAL_NVP(sizes));
  }

  template<typename Ar>
  void save(Ar & archive) const
  {
    archive(CEREAL_NVP(repetitions));
    archive(CEREAL_NVP(sizes));
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
