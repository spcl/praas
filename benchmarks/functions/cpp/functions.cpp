
#include <praas/function/invocation.hpp>
#include <praas/function/context.hpp>

#include <chrono>

#include "types.hpp"

extern "C" int no_op(praas::function::Invocation invocation, praas::function::Context& context)
{
  // Return the data
  context.set_output_buffer(invocation.args[0]);
  return 0;
}

extern "C" int no_op_local(praas::function::Invocation invocation, praas::function::Context& context)
{
  std::cerr << "Start benchmark" << std::endl;
  Invocations in;
  invocation.args[0].deserialize(in);
  std::cerr << "Benchmark " << in.repetitions << " " << in.sizes[0] << std::endl;

  Results res;
  auto buf = context.get_buffer(in.sizes.back());
    for(int i = 0; i < in.sizes.back() / 4; ++i)
      ((int*)buf.ptr)[i] = i + 1;

  for(int size : in.sizes) {

    std::cout << "Begin size " << size << std::endl;
    buf.len = size;

    res.measurements.emplace_back();
    auto my_id = context.process_id();
    for(int i = 0; i  < in.repetitions; ++i) {

      auto begin = std::chrono::high_resolution_clock::now();
      auto result = context.invoke(my_id, "no_op", "local-id", buf);
      auto end = std::chrono::high_resolution_clock::now();

      if(result.payload.len != size) {
        std::cerr << "Incorrect return size " << result.payload.len << std::endl;
        return 1;
      }

      res.measurements.back().emplace_back(
        std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count()
      );

    }

  }

  auto& output_buf = context.get_output_buffer(in.repetitions * in.sizes.size() * sizeof(long) + 64);
  output_buf.serialize(res);
  return 0;
}
