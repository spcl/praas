#include <praas/control-plane/backend.hpp>

#include <praas/control-plane/config.hpp>
#include <praas/common/exceptions.hpp>

namespace praas::control_plane::backend {

  Type deserialize(std::string mode)
  {
    if (mode == "local") {
      return Type::LOCAL;
    } else if (mode == "aws_fargate") {
      return Type::AWS_FARGATE;
    } else {
      return Type::NONE;
    }
  }

  std::unique_ptr<Backend> Backend::construct(const config::Config& cfg)
  {
    if(cfg.backend_type == Type::LOCAL) {
      return std::make_unique<LocalBackend>();
    }
    return nullptr;
  }

  void LocalBackend::allocate_process(process::ProcessPtr, const process::Resources& resources)
  {
    throw praas::common::NotImplementedError{};
  }

  int LocalBackend::max_memory() const
  {
    throw praas::common::NotImplementedError{};
  }

  int LocalBackend::max_vcpus() const
  {
    throw praas::common::NotImplementedError{};
  }

} // namespace praas::control_plane::backend
