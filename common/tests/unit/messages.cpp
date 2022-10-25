
#include <praas/common/messages.hpp>

#include <praas/common/exceptions.hpp>

#include <gtest/gtest.h>

using namespace praas::common::message;

TEST(Messages, ProcessConnectionMsg)
{
  {
    std::string process_name{"test-name"};

    ProcessConnection req;;
    req.process_name(process_name);

    EXPECT_EQ(req.process_name(), process_name);
    EXPECT_EQ(req.type(), Message::Type::PROCESS_CONNECTION);
  }

  {
    std::string process_name(ProcessConnection::NAME_LENGTH, 't');

    ProcessConnection req;
    req.process_name(process_name);

    EXPECT_EQ(req.process_name(), process_name);
    EXPECT_EQ(req.process_name().length(), ProcessConnection::NAME_LENGTH);
  }
}

TEST(Messages, ProcessConnectionMsgIncorrect)
{
  std::string process_name(ProcessConnection::NAME_LENGTH + 1, 't');

  ProcessConnection req;
  EXPECT_THROW(req.process_name(process_name), praas::common::InvalidArgument);
}

TEST(Messages, ProcessConnectionMsgParse)
{
  std::string process_name{"test-name"};
  ProcessConnection req;
  req.process_name(process_name);

  Message& msg = *static_cast<Message*>(&req);
  auto parsed = msg.parse();

  EXPECT_TRUE(std::holds_alternative<ProcessConnectionParsed>(parsed));

  EXPECT_TRUE(std::visit(
      overloaded{[](ProcessConnectionParsed&) { return true; }, [](auto&) { return false; }}, parsed
  ));
}

TEST(Messages, SwapConfirmationMsg)
{
  SwapConfirmation req;

  EXPECT_EQ(req.type(), Message::Type::SWAP_CONFIRMATION);
}

TEST(Messages, SwapConfirmationMsgParse)
{
  SwapConfirmation req;

  Message& msg = *static_cast<Message*>(&req);
  auto parsed = msg.parse();

  EXPECT_TRUE(std::holds_alternative<SwapConfirmationParsed>(parsed));

  EXPECT_TRUE(std::visit(
      overloaded{
          [](SwapConfirmationParsed&) { return true; },
          [](ProcessConnectionParsed&) { return false; }, [](auto&) { return false; }},
      parsed
  ));
}

TEST(Messages, InvocationRequestMsg)
{
  {
    std::string invoc_id{"invoc-id-42"};
    std::string fname{"test-name"};
    int32_t payload_size{32};

    InvocationRequest req;
    req.invocation_id(invoc_id);
    req.function_name(fname);
    req.payload_size(payload_size);

    EXPECT_EQ(req.invocation_id(), invoc_id);
    EXPECT_EQ(req.payload_size(), payload_size);
    EXPECT_EQ(req.function_name(), fname);
    EXPECT_EQ(req.type(), Message::Type::INVOCATION_REQUEST);
  }

  {
    std::string invoc_id(ProcessConnection::ID_LENGTH, 't');
    std::string fname(ProcessConnection::NAME_LENGTH, 'a');
    int32_t payload_size{0};

    InvocationRequest req;
    req.invocation_id(invoc_id);
    req.payload_size(payload_size);
    req.function_name(fname);

    EXPECT_EQ(req.invocation_id(), invoc_id);
    EXPECT_EQ(req.payload_size(), payload_size);
    EXPECT_EQ(req.function_name(), fname);
  }
}

TEST(Messages, InvocationRequestMsgIncorrect)
{
  {
    std::string invoc_id(ProcessConnection::ID_LENGTH + 1, 't');

    InvocationRequest req;
    EXPECT_THROW(req.invocation_id(invoc_id), praas::common::InvalidArgument);
  }

  {
    std::string fname(ProcessConnection::NAME_LENGTH + 1, 'c');

    InvocationRequest req;
    EXPECT_THROW(req.function_name(fname), praas::common::InvalidArgument);
  }

  {
    InvocationRequest req;
    EXPECT_THROW(req.payload_size(-1), praas::common::InvalidArgument);
  }
}

TEST(Messages, InvocationRequestMsgParse)
{
  std::string invoc_id{"invoc-id-42"};
    std::string fname{"test-name"};
  int32_t payload_size{32};

  InvocationRequest req;
  req.invocation_id(invoc_id);
  req.payload_size(payload_size);
  req.function_name(fname);

  Message& msg = *static_cast<Message*>(&req);
  auto parsed = msg.parse();

  EXPECT_TRUE(std::holds_alternative<InvocationRequestParsed>(parsed));

  EXPECT_TRUE(std::visit(
      overloaded{
          [](InvocationRequestParsed&) { return true; },
          [](SwapConfirmationParsed&) { return false; },
          [](ProcessConnectionParsed&) { return false; },
          [](auto&) { return false; }},
      parsed
  ));
}

TEST(Messages, InvocationResultMsg)
{
  {
    std::string invoc_id{"invoc-id-42"};
    int32_t payload_size{32};

    InvocationResult req;
    req.invocation_id(invoc_id);
    req.response_size(payload_size);

    EXPECT_EQ(req.invocation_id(), invoc_id);
    EXPECT_EQ(req.response_size(), payload_size);
    EXPECT_EQ(req.type(), Message::Type::INVOCATION_RESULT);
  }

  {
    std::string invoc_id(ProcessConnection::NAME_LENGTH, 't');
    int32_t payload_size{0};

    InvocationResult req;
    req.invocation_id(invoc_id);
    req.response_size(payload_size);

    EXPECT_EQ(req.invocation_id(), invoc_id);
    EXPECT_EQ(req.response_size(), payload_size);
  }
}

TEST(Messages, InvocationResultMsgIncorrect)
{
  {
    std::string invoc_id(ProcessConnection::NAME_LENGTH + 1, 't');

    InvocationResult req;
    EXPECT_THROW(req.invocation_id(invoc_id), praas::common::InvalidArgument);
  }

  {
    InvocationResult req;
    EXPECT_THROW(req.response_size(-1), praas::common::InvalidArgument);
  }
}

TEST(Messages, InvocationResultMsgParse)
{
  std::string invoc_id{"invoc-id-42"};
  int32_t payload_size{32};

  InvocationResult req;
  req.invocation_id(invoc_id);
  req.response_size(payload_size);

  Message& msg = *static_cast<Message*>(&req);
  auto parsed = msg.parse();

  EXPECT_TRUE(std::holds_alternative<InvocationResultParsed>(parsed));

  EXPECT_TRUE(std::visit(
      overloaded{
          [](InvocationResultParsed&) { return true; },
          [](InvocationRequestParsed&) { return false; },
          [](SwapConfirmationParsed&) { return false; },
          [](ProcessConnectionParsed&) { return false; },
          [](auto&) { return false; }},
      parsed
  ));
}

TEST(Messages, DataPlaneMetricsMsg)
{
  {
    int32_t invocations{0};
    int32_t computation_time{0};
    uint64_t timestamp = 100;

    DataPlaneMetrics req;
    req.invocations(invocations);
    req.computation_time(computation_time);
    req.last_invocation_timestamp(timestamp);

    EXPECT_EQ(req.invocations(), invocations);
    EXPECT_EQ(req.computation_time(), computation_time);
    EXPECT_EQ(req.last_invocation_timestamp(), timestamp);
    EXPECT_EQ(req.type(), Message::Type::DATAPLANE_METRICS);
  }
}

TEST(Messages, DataPlaneMetricsMsgIncorrect)
{
  {
    DataPlaneMetrics req;
    EXPECT_THROW(req.invocations(-5), praas::common::InvalidArgument);
  }

  {
    DataPlaneMetrics req;
    EXPECT_THROW(req.computation_time(-5), praas::common::InvalidArgument);
  }
}

TEST(Messages, DataPlaneMetricsMsgParse)
{
  int32_t invocations{0};
  int32_t computation_time{0};
  uint64_t timestamp = 100;

  DataPlaneMetrics req;
  req.invocations(invocations);
  req.computation_time(computation_time);
  req.last_invocation_timestamp(timestamp);

  Message& msg = *static_cast<Message*>(&req);
  auto parsed = msg.parse();

  EXPECT_TRUE(std::holds_alternative<DataPlaneMetricsParsed>(parsed));

  EXPECT_TRUE(std::visit(
      overloaded{
          [](DataPlaneMetricsParsed&) { return true; },
          [](InvocationResultParsed&) { return false; },
          [](InvocationRequestParsed&) { return false; },
          [](SwapConfirmationParsed&) { return false; },
          [](ProcessConnectionParsed&) { return false; },
          [](auto&) { return false; }},
      parsed
  ));
}

TEST(Messages, ProcessClosureMsg)
{
  ProcessClosure req;

  EXPECT_EQ(req.type(), Message::Type::PROCESS_CLOSURE);
}

TEST(Messages, ProcessClosureMsgParse)
{
  ProcessClosure req;

  Message& msg = *static_cast<Message*>(&req);
  auto parsed = msg.parse();

  EXPECT_TRUE(std::holds_alternative<ProcessClosureParsed>(parsed));

  EXPECT_TRUE(std::visit(
      overloaded{
          [](ProcessClosureParsed&) { return true; },
          [](DataPlaneMetricsParsed&) { return false; },
          [](InvocationResultParsed&) { return false; },
          [](InvocationRequestParsed&) { return false; },
          [](SwapConfirmationParsed&) { return false; },
          [](ProcessConnectionParsed&) { return false; },
          [](auto&) { return false; }},
      parsed
  ));
}
