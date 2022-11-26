#include "test.hpp"

#include <praas/function/invocation.hpp>
#include <praas/function/context.hpp>

#include <iostream>

#include <boost/iostreams/stream.hpp>

extern "C" void add(praas::function::Invocation invocation, praas::function::Context&)
{
  Input in;

  auto & arg = invocation.args[0];
  boost::iostreams::stream<boost::iostreams::array_source> stream(arg.val, arg.size);
  cereal::BinaryInputArchive archive_in{stream};
  in.load(archive_in);

  std::cerr << in.arg1 << " " << in.arg2 << std::endl;
}
