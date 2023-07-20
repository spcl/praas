
#include <praas/process/runtime/context.hpp>
#include <praas/process/runtime/invocation.hpp>

#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>

#include <string>

struct Result {
  std::string message;

  template <typename Ar>
  void serialize(Ar& archive)
  {
    archive(CEREAL_NVP(message));
  }
};

extern "C" int
no_op(praas::process::runtime::Invocation invocation, praas::process::runtime::Context& context)
{
  Result res{"Hello, world!"};
  auto& output_buf = context.get_output_buffer(1024);
  output_buf.serialize(res);
  // Return the data
  context.set_output_buffer(invocation.args[0]);
  return 0;
}
