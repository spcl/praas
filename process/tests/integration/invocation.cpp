
#include <praas/common/exceptions.hpp>
#include <praas/process/controller/config.hpp>
#include <praas/process/controller/controller.hpp>
#include <praas/process/runtime/ipc/messages.hpp>
#include <praas/common/messages.hpp>

#include "examples/cpp/test.hpp"

#include <boost/iostreams/device/array.hpp>
#include <thread>

#include <boost/iostreams/stream.hpp>
#include <boost/interprocess/streams/bufferstream.hpp>
#include <cereal/archives/binary.hpp>
#include <gtest/gtest.h>

using namespace praas::process;

TEST(ProcessInvocationTest, SimpleInvocation)
{
  config::Controller cfg;
  cfg.set_defaults();

  cfg.verbose = true;
  //FIXME: compiler time defaults
  cfg.code.location = "/work/serverless/2022/praas/code/praas/process/tests/integration";
  cfg.code.config_location = "configuration.json";

  Controller controller{cfg};
  std::thread controller_thread{&Controller::start, &controller};

  runtime::BufferQueue<char> buffers(10, 1024);
  praas::common::message::InvocationRequest msg;
  msg.function_name("test");
  msg.invocation_id("id");
  msg.payload_size(300);

  auto buf = buffers.retrieve_buffer(300);
  Input input{42, 2};
  boost::interprocess::bufferstream stream(buf.val, buf.size);
  cereal::BinaryOutputArchive archive_out{stream};
  archive_out(input);
  assert(stream.good());
  size_t pos = stream.tellp();
  buf.len = pos;
  std::cerr << "pos " << stream.tellp() << std::endl;

  controller.wakeup(std::move(msg), std::move(buf));

  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  controller.wakeup(std::move(msg), std::move(buf));
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  //ipc::InvocationRequest req;
  //req.invocation_id("test");
  //req.function_name("func");
  //std::array<int, 1> buffers_lens{300};
  //req.buffers(buffers_lens.begin(), buffers_lens.end());
  //buf.len = 10;
  //buffers.return_buffer(buf);

  controller.shutdown();
  controller_thread.join();
}
