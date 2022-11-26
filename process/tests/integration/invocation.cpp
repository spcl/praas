
#include <praas/common/exceptions.hpp>
#include <praas/process/controller/config.hpp>
#include <praas/process/controller/controller.hpp>
#include <praas/process/ipc/messages.hpp>

#include <thread>

#include <gtest/gtest.h>
#include "praas/common/messages.hpp"

using namespace praas::process;

TEST(ProcessInvocationTest, SimpleInvocation)
{
  config::Controller cfg;
  cfg.set_defaults();

  cfg.verbose = true;
  //FIXME: compiler time defaults
  cfg.code.location = "/work/serverless/2022/praas/code/praas/process/config";
  cfg.code.config_location = "example_functions.json";

  Controller controller{cfg};
  std::thread controller_thread{&Controller::start, &controller};

  ipc::BufferQueue<char> buffers(10, 1024);
  praas::common::message::InvocationRequest msg;
  msg.function_name("test");
  msg.invocation_id("id");
  msg.payload_size(300);

  auto buf = buffers.retrieve_buffer(300);

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
