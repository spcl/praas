#include "test.hpp"

#include <praas/function/invocation.hpp>
#include <praas/function/context.hpp>

#include <iostream>

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

extern "C" int send_message(praas::function::Invocation invoc, praas::function::Context& context)
{
  constexpr int MSG_SIZE = 1024;
  praas::function::Buffer buf = context.get_buffer(MSG_SIZE);

  InputMsgKey key;
  invoc.args[0].deserialize(key);

  Message msg;
  msg.some_data = 42;
  msg.message = "THIS IS A TEST MESSAGE";
  buf.serialize(msg);

  context.put(praas::function::Context::SELF, key.message_key, buf.ptr, buf.len);

  return 0;
}

int get_message(praas::function::Invocation invoc, std::string_view source, praas::function::Context& context)
{
  InputMsgKey key;
  invoc.args[0].deserialize(key);

  praas::function::Buffer buf = context.get(source, key.message_key);

  Message msg;
  buf.deserialize(msg);

  if(msg.some_data != 42 || msg.message != "THIS IS A TEST MESSAGE") {
    return 1;
  } else {
    return 0;
  }
}

extern "C" int get_message_self(praas::function::Invocation invoc, praas::function::Context& context)
{
  return get_message(invoc, praas::function::Context::SELF, context);
}

extern "C" int get_message_any(praas::function::Invocation invoc, praas::function::Context& context)
{
  return get_message(invoc, praas::function::Context::ANY, context);
}

extern "C" int get_message_explicit(praas::function::Invocation invoc, praas::function::Context& context)
{
  return get_message(invoc, context.process_id(), context);
}

extern "C" int get_message(praas::function::Invocation invoc, praas::function::Context& context)
{
  return get_message(invoc, context.process_id(), context);
}

// Computes arg1 ** arg2
extern "C" int power(praas::function::Invocation invocation, praas::function::Context& context)
{
  Input in{};
  Output out{};

  invocation.args[0].deserialize(in);

  if(in.arg2 > 2) {
    Input invoc_in{in.arg1, in.arg2 - 1};
    auto buf = context.get_buffer(1024);
    buf.serialize(invoc_in);

    praas::function::InvocationResult invoc_result
      = context.invoke(context.process_id(), "power", "second_add" + std::to_string(in.arg2-1), buf);
    invoc_result.payload.deserialize(out);
    out.result *= in.arg1;
  } else {
    out.result = in.arg1 * in.arg1;
  }

  // Serialize again just for validation
  auto& output_buf = context.get_output_buffer();
  output_buf.serialize(out);

  return 0;
}

extern "C" int send_remote_message(praas::function::Invocation invoc, praas::function::Context& context)
{
  std::cerr << context.active_processes().size() << std::endl;
  for(int i = 0; i < context.active_processes().size(); ++i)
    std::cerr << context.active_processes()[i] << std::endl;
  std::string other_process_id;
  if(context.active_processes()[0] == context.process_id()) {
    other_process_id = context.active_processes()[1];
  } else {
    other_process_id = context.active_processes()[0];
  }

  constexpr int MSG_SIZE = 1024;
  praas::function::Buffer buf = context.get_buffer(MSG_SIZE);

  InputMsgKey key;
  invoc.args[0].deserialize(key);

  Message msg;
  msg.some_data = 42;
  msg.message = "THIS IS A TEST MESSAGE";
  buf.serialize(msg);

  context.put(other_process_id, key.message_key, buf.ptr, buf.len);

  return 0;
}

extern "C" int get_remote_message(praas::function::Invocation invoc, praas::function::Context& context)
{
  std::cerr << context.active_processes().size() << std::endl;
  for(int i = 0; i < context.active_processes().size(); ++i)
    std::cerr << context.active_processes()[i] << std::endl;

  InputMsgKey key;
  invoc.args[0].deserialize(key);

  praas::function::Buffer buf = context.get(praas::function::Context::ANY, key.message_key);

  Message msg;
  buf.deserialize(msg);
  std::cerr << msg.some_data << " " << msg.message << std::endl;

  if(msg.some_data != 42 || msg.message != "THIS IS A TEST MESSAGE") {
    return 1;
  } else {
    return 0;
  }
}

