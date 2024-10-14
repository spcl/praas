
#include <praas/process/runtime/context.hpp>
#include <praas/process/runtime/invocation.hpp>

#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>

#include <fstream>
#include <filesystem>
#include <ios>
#include <string>

struct IO {
  std::string message;

  template<typename Ar>
  void save(Ar & archive) const
  {
    archive(CEREAL_NVP(message));
  }

  template<typename Ar>
  void load(Ar & archive)
  {
    archive(CEREAL_NVP(message));
  }
};

extern "C" int state_put(
  praas::process::runtime::Invocation invocation, praas::process::runtime::Context& context
)
{
  IO msg;
  invocation.args[0].deserialize(msg);

  praas::process::runtime::Buffer buf = context.state("STATE_MSG_1");

  auto send_buf = context.get_buffer(1024);
  send_buf.serialize(msg);

  context.state("STATE_MSG_1", send_buf);

  msg.message = "second_state_msg";
  send_buf.serialize(msg);
  context.state("STATE_MSG_2", send_buf);

  msg.message = "42424242";
  send_buf.serialize(msg);
  context.state("STATE_MSG_3", send_buf);

  std::filesystem::create_directories("/state/test1/test2/");
  std::ofstream out{"/state/test1/test2/TEST"};
  out << "newfiledata";
  out.close();

  return 0;
}

extern "C" int state_get(
  praas::process::runtime::Invocation invocation, praas::process::runtime::Context& context
)
{
  IO key;
  praas::process::runtime::Buffer buf = context.state("STATE_MSG_1");
  buf.deserialize(key);

  auto& output_buf = context.get_output_buffer(1024);
  output_buf.serialize(key);
  context.set_output_buffer(output_buf);

  return 0;
}
