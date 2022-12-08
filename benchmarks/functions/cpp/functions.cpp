
#include <praas/function/invocation.hpp>
#include <praas/function/context.hpp>

extern "C" int no_op(praas::function::Invocation invocation, praas::function::Context& context)
{
  // Return the data
  context.set_output_buffer(invocation.args[0]);
  return 0;
}
