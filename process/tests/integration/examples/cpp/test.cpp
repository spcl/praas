#include "test.hpp"

#include <praas/function/invocation.hpp>
#include <praas/function/context.hpp>

extern "C" int add(praas::function::Invocation invocation, praas::function::Context& context)
{
  Input in{};
  Output out{};

  invocation.args[0].deserialize(in);

  out.result = in.arg1 + in.arg2;

  auto& buf = context.get_output_buffer();
  buf.serialize(out);

  return 0;
}

extern "C" int zero_return(praas::function::Invocation /*unused*/, praas::function::Context& /*unused*/)
{
  return 0;
}

extern "C" int error_function(praas::function::Invocation /*unused*/, praas::function::Context& /*unused*/)
{
  return 1;
}

extern "C" int large_payload(praas::function::Invocation invocation, praas::function::Context& context)
{
  size_t len = invocation.args[0].len / sizeof(int);
  int* input = reinterpret_cast<int*>(invocation.args[0].ptr);

  auto& out_buf = context.get_output_buffer(len * sizeof(int));
  int* output = reinterpret_cast<int*>(out_buf.ptr);

  for(size_t i = 0; i < len; ++i) {

    output[i] = input[i] + 2;

  }

  out_buf.len = len * sizeof(int);

  return 0;
}

extern "C" int send_message(praas::function::Invocation /*unused*/, praas::function::Context& context)
{
  constexpr int MSG_SIZE = 1024;
  praas::function::Buffer buf = context.get_buffer(MSG_SIZE);

  Message msg;
  msg.some_data = 42;
  msg.message = "THIS IS A TEST MESSAGE";
  buf.serialize(msg);

  context.put(praas::function::Context::SELF, "test_msg", buf.ptr, buf.len);

  return 0;
}

int get_message(std::string_view source, praas::function::Context& context)
{
  praas::function::Buffer buf = context.get(source, "test_msg");

  Message msg;
  buf.deserialize(msg);

  if(msg.some_data != 42 || msg.message != "THIS IS A TEST MESSAGE") {
    return 1;
  } else {
    return 0;
  }
}

extern "C" int get_message_self(praas::function::Invocation /*unused*/, praas::function::Context& context)
{
  return get_message(praas::function::Context::SELF, context);
}

extern "C" int get_message_any(praas::function::Invocation /*unused*/, praas::function::Context& context)
{
  return get_message(praas::function::Context::ANY, context);
}

extern "C" int get_message_explicit(praas::function::Invocation /*unused*/, praas::function::Context& context)
{
  return get_message(context.process_id(), context);
}

