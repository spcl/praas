#include <praas/control-plane/config.hpp>

#include <cereal/archives/json.hpp>

namespace praas::control_plane::config {

  Config Config::deserialize(std::istream& in_stream)
  {
    Config cfg;
    cereal::JSONInputArchive archive_in(in_stream);
    cfg.load(archive_in);
    return cfg;
  }

  void Config::load(cereal::JSONInputArchive& archive)
  {
    archive(CEREAL_NVP(verbose));
  }

} // namespace praas::control_plane::config
