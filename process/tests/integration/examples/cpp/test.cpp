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

extern "C" int state(praas::function::Invocation invoc, praas::function::Context& context)
{
  InputMsgKey key;
  invoc.args[0].deserialize(key);

  praas::function::Buffer buf = context.state(key.message_key);
  std::cerr << buf.ptr << " " << buf.len << std::endl;
  if(buf.len != 0) {
    return 1;
  }

  auto send_buf = context.get_buffer(1024);
  ((int*)send_buf.ptr)[0] = 42;
  ((int*)send_buf.ptr)[1] = 33;
  send_buf.len = sizeof(int)*2;

  context.state(key.message_key, send_buf);

  // Now get
  auto rcv_buf = context.state(key.message_key);
  std::cerr << rcv_buf.ptr << " " << rcv_buf.len << std::endl;
  if(!rcv_buf.ptr || rcv_buf.len != sizeof(int)*2) {
    return 1;
  }
  auto ptr = ((int*)rcv_buf.ptr);
  if(ptr[0] != 42 || ptr[1] != 33)
    return 1;

  // Now get once more
  auto rcv_buf2 = context.state(key.message_key);
  std::cerr << rcv_buf2.ptr << " " << rcv_buf2.len << std::endl;
  if(!rcv_buf2.ptr || rcv_buf2.len != sizeof(int)*2) {
    return 1;
  }
  ptr = ((int*)rcv_buf2.ptr);
  if(ptr[0] != 42 || ptr[1] != 33)
    return 1;

  return 0;
}

extern "C" int state_get(praas::function::Invocation invoc, praas::function::Context& context)
{
  InputMsgKey key;
  invoc.args[0].deserialize(key);

  // Now get
  auto rcv_buf = context.state(key.message_key);
  std::cerr << rcv_buf.ptr << " " << rcv_buf.len << std::endl;
  if(!rcv_buf.ptr || rcv_buf.len != sizeof(int)*2) {
    return 1;
  }
  auto ptr = ((int*)rcv_buf.ptr);
  if(ptr[0] != 42 || ptr[1] != 33)
    return 1;

  return 0;
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
  std::string other_process_id;
  if(context.active_processes()[0] == context.process_id()) {
    other_process_id = context.active_processes()[1];
  } else {
    other_process_id = context.active_processes()[0];
  }

  constexpr int MSG_SIZE = 1024;
  praas::function::Buffer buf = context.get_buffer(MSG_SIZE);
  praas::function::Buffer second_buf = context.get_buffer(MSG_SIZE);

  InputMsgKey key;
  invoc.args[0].deserialize(key);

  Message msg;
  msg.some_data = 42;
  msg.message = "THIS IS A TEST MESSAGE";
  buf.serialize(msg);

  Message second_msg;
  second_msg.some_data = 33;
  second_msg.message = "THIS IS A SECOND TEST MESSAGE";
  second_buf.serialize(second_msg);

  context.put(other_process_id, key.message_key, buf.ptr, buf.len);
  context.put(other_process_id, key.message_key + std::to_string(2), second_buf.ptr, second_buf.len);

  return 0;
}

extern "C" int get_remote_message(praas::function::Invocation invoc, praas::function::Context& context)
{
  std::string other_process_id;
  if(context.active_processes()[0] == context.process_id()) {
    other_process_id = context.active_processes()[1];
  } else {
    other_process_id = context.active_processes()[0];
  }

  InputMsgKey key;
  invoc.args[0].deserialize(key);

  praas::function::Buffer buf = context.get(praas::function::Context::ANY, key.message_key);

  Message msg;
  buf.deserialize(msg);

  if(msg.some_data != 42 || msg.message != "THIS IS A TEST MESSAGE") {
    return 1;
  }

  buf = context.get(other_process_id, key.message_key + std::to_string(2));
  buf.deserialize(msg);

  if(msg.some_data != 33 || msg.message != "THIS IS A SECOND TEST MESSAGE") {
    return 1;
  } else {
    return 0;
  }
}

extern "C" int put_get_remote_message(praas::function::Invocation invoc, praas::function::Context& context)
{
  std::string other_process_id;
  if(context.active_processes()[0] == context.process_id()) {
    other_process_id = context.active_processes()[1];
  } else {
    other_process_id = context.active_processes()[0];
  }

  InputMsgKey key;
  invoc.args[0].deserialize(key);

  praas::function::Buffer buf = context.get_buffer(1024);
  Message message;
  message.some_data = 22;
  message.message = "MESSAGE " + other_process_id;
  buf.serialize(message);

  context.put(other_process_id, key.message_key, buf.ptr, buf.len);

  praas::function::Buffer get_buf = context.get(praas::function::Context::ANY, key.message_key);

  Message msg;
  get_buf.deserialize(msg);

  if(msg.some_data != 22 || msg.message != "MESSAGE " + context.process_id()) {
    return 1;
  }

  return 0;
}

extern "C" int remote_invocation(praas::function::Invocation invocation, praas::function::Context& context)
{
  std::string other_process_id;
  if(context.active_processes()[0] == context.process_id()) {
    other_process_id = context.active_processes()[1];
  } else {
    other_process_id = context.active_processes()[0];
  }

  Output out{};
  Input input{};
  invocation.args[0].deserialize(input);

  auto buf = context.get_buffer(1024);
  buf.serialize(input);
  std::cerr << "Invoke add" << std::endl;
  praas::function::InvocationResult invoc_result
    = context.invoke(other_process_id, "add", "second_add" , buf);
  invoc_result.payload.deserialize(out);

  out.result *= 2;

  // Serialize again just for validation
  auto& output_buf = context.get_output_buffer();
  output_buf.serialize(out);

  return 0;
}

