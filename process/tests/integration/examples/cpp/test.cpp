#include "test.hpp"

#include <praas/function/invocation.hpp>
#include <praas/function/context.hpp>

#include <iostream>

#include <boost/iostreams/stream.hpp>
#include <boost/interprocess/streams/bufferstream.hpp>

extern "C" int add(praas::function::Invocation invocation, praas::function::Context& context)
{
  Input in;

  std::cerr << "INVOKE" << std::endl;
  auto & arg = invocation.args[0];
  boost::iostreams::stream<boost::iostreams::array_source> stream(arg.val, arg.size);
  cereal::BinaryInputArchive archive_in{stream};
  in.load(archive_in);

  std::cerr << in.arg1 << " " << in.arg2 << std::endl;

  Output out;
  out.result = in.arg1 + in.arg2;

  auto buf = context.get_output_buffer();
  boost::interprocess::bufferstream out_stream(reinterpret_cast<char*>(buf.ptr), buf.size);
  cereal::BinaryOutputArchive archive_out{out_stream};
  archive_out(out);

  std::cerr << out_stream.tellp() << std::endl;
  context.set_output_len(out_stream.tellp());

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
  int* input = reinterpret_cast<int*>(invocation.args[0].val);

  auto out_buf = context.get_output_buffer(len * sizeof(int));
  int* output = reinterpret_cast<int*>(out_buf.ptr);

  for(size_t i = 0; i < len; ++i) {

    *output = *input + 2;

  }

  return 0;
}
