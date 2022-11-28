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
