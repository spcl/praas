
#include <praas/common/messages.hpp>

#include <praas/common/exceptions.hpp>

#include <gtest/gtest.h>

using namespace praas::common::message;

TEST(Messages, ProcessConnectionMsg)
{
  {
    std::string process_name{"test-name"};

    ProcessConnection<MessageData> req;
    req.process_name(process_name);

    EXPECT_EQ(req.process_name(), process_name);
    EXPECT_EQ(req.type(), MessageType::PROCESS_CONNECTION);
  }

  {
    std::string process_name(MessageConfig::NAME_LENGTH, 't');

    ProcessConnection<MessageData> req;
    req.process_name(process_name);

    EXPECT_EQ(req.process_name(), process_name);
    EXPECT_EQ(req.process_name().length(), MessageConfig::NAME_LENGTH);
  }
}

TEST(Messages, ProcessConnectionMsgIncorrect)
{
  std::string process_name(MessageConfig::NAME_LENGTH + 1, 't');

  ProcessConnection<MessageData> req;
  EXPECT_THROW(req.process_name(process_name), praas::common::InvalidArgument);
}

TEST(Messages, ProcessConnectionMsgParse)
{
  std::string process_name{"test-name"};
  ProcessConnection<MessageData> req;
  req.process_name(process_name);

  auto parsed = MessageParser::parse(req.ptr());

  EXPECT_TRUE(std::holds_alternative<ProcessConnectionPtr>(parsed));

  EXPECT_TRUE(std::visit(
      overloaded{[](ProcessConnectionPtr&) { return true; }, [](auto&) { return false; }}, parsed
  ));
}

TEST(Messages, SwapRequestMsg)
{

  {
    std::string swap_loc{"test-name"};

    SwapRequestData req;
    req.path(swap_loc);

    EXPECT_EQ(req.type(), MessageType::SWAP_REQUEST);
    EXPECT_EQ(req.path(), swap_loc);
  }

  {
    std::string swap_loc(MessageConfig::ID_LENGTH, 't');

    SwapRequestData req;
    req.path(swap_loc);

    EXPECT_EQ(req.type(), MessageType::SWAP_REQUEST);
    EXPECT_EQ(req.path(), swap_loc);
    EXPECT_EQ(req.path().length(), MessageConfig::ID_LENGTH);
  }
}

TEST(Messages, SwapRequestMsgIncorrect)
{
  std::string swap_loc(MessageConfig::ID_LENGTH + 1, 't');

  SwapRequest<MessageData> req;
  EXPECT_THROW(req.path(swap_loc), praas::common::InvalidArgument);
}

TEST(Messages, SwapRequestMsgParse)
{
  SwapRequest<MessageData> req;

  auto parsed = MessageParser::parse(req.ptr());

  EXPECT_TRUE(std::holds_alternative<SwapRequestPtr>(parsed));

  EXPECT_TRUE(std::visit(
      overloaded{[](SwapRequestPtr&) { return true; }, [](auto&) { return false; }}, parsed
  ));
}


TEST(Messages, SwapConfirmationMsg)
{
  int32_t swap_size{32};

  SwapConfirmation<MessageData> req;
  req.swap_size(swap_size);

  EXPECT_EQ(req.type(), MessageType::SWAP_CONFIRMATION);
  EXPECT_EQ(req.swap_size(), swap_size);
}

TEST(Messages, SwapConfirmationMsgParse)
{
  SwapConfirmation<MessageData> req;

  auto parsed = MessageParser::parse(req.ptr());

  EXPECT_TRUE(std::holds_alternative<SwapConfirmationPtr>(parsed));

  EXPECT_TRUE(std::visit(
      overloaded{
          [](SwapConfirmationPtr&) { return true; },
          [](auto&) { return false; }},
      parsed
  ));
}


TEST(Messages, InvocationRequestMsg)
{
  {
    std::string invoc_id{"invoc-id-42"};
    std::string fname{"test-name"};
    int32_t payload_size{32};

    InvocationRequestData req;
    req.invocation_id(invoc_id);
    req.function_name(fname);
    req.payload_size(payload_size);

    EXPECT_EQ(req.invocation_id(), invoc_id);
    EXPECT_EQ(req.payload_size(), payload_size);
    EXPECT_EQ(req.function_name(), fname);
    EXPECT_EQ(req.type(), MessageType::INVOCATION_REQUEST);
  }

  {
    std::string invoc_id(MessageConfig::ID_LENGTH, 't');
    std::string fname(MessageConfig::NAME_LENGTH, 'a');
    int32_t payload_size{0};

    InvocationRequestData req;
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
    std::string invoc_id(MessageConfig::ID_LENGTH + 1, 't');

    InvocationRequestData req;
    EXPECT_THROW(req.invocation_id(invoc_id), praas::common::InvalidArgument);
  }

  {
    std::string fname(MessageConfig::NAME_LENGTH + 1, 'c');

    InvocationRequestData req;
    EXPECT_THROW(req.function_name(fname), praas::common::InvalidArgument);
  }

  {
    InvocationRequestData req;
    EXPECT_THROW(req.payload_size(-1), praas::common::InvalidArgument);
  }
}

TEST(Messages, InvocationRequestMsgParse)
{
  std::string invoc_id{"invoc-id-42"};
  std::string fname{"test-name"};
  int32_t payload_size{32};

  InvocationRequestData req;
  req.invocation_id(invoc_id);
  req.payload_size(payload_size);
  req.function_name(fname);

  auto parsed = MessageParser::parse(req.ptr());

  EXPECT_TRUE(std::holds_alternative<InvocationRequestPtr>(parsed));

  EXPECT_TRUE(std::visit(
      overloaded{
          [](InvocationRequestPtr&) { return true; },
          [](auto&) { return false; }},
      parsed
  ));
}

TEST(Messages, InvocationResultMsg)
{
  {
    std::string invoc_id{"invoc-id-42"};
    int32_t payload_size{32};

    InvocationResultData req;
    req.invocation_id(invoc_id);
    req.total_length(payload_size);
    req.return_code(0);

    EXPECT_EQ(req.invocation_id(), invoc_id);
    EXPECT_EQ(req.total_length(), payload_size);
    EXPECT_EQ(req.return_code(), 0);
    EXPECT_EQ(req.type(), MessageType::INVOCATION_RESULT);
  }

  {
    std::string invoc_id(MessageConfig::ID_LENGTH, 't');
    int32_t payload_size{0};

    InvocationResultData req;
    req.invocation_id(invoc_id);
    req.total_length(payload_size);
    req.return_code(4);

    EXPECT_EQ(req.invocation_id(), invoc_id);
    EXPECT_EQ(req.total_length(), payload_size);
    EXPECT_EQ(req.return_code(), 4);
  }
}

TEST(Messages, InvocationResultMsgIncorrect)
{
  {
    std::string invoc_id(MessageConfig::ID_LENGTH + 1, 't');

    InvocationResultData req;
    EXPECT_THROW(req.invocation_id(invoc_id), praas::common::InvalidArgument);
  }
}

TEST(Messages, InvocationResultMsgParse)
{
  std::string invoc_id{"invoc-id-42"};
  int32_t payload_size{32};

  InvocationResultData req;
  req.invocation_id(invoc_id);
  req.total_length(payload_size);
  req.return_code(0);

  auto parsed = MessageParser::parse(req.ptr());

  EXPECT_TRUE(std::holds_alternative<InvocationResultPtr>(parsed));

  EXPECT_TRUE(std::visit(
      overloaded{
          [](InvocationResultPtr&) { return true; },
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

    DataPlaneMetricsData req;
    req.invocations(invocations);
    req.computation_time(computation_time);
    req.last_invocation_timestamp(timestamp);

    EXPECT_EQ(req.invocations(), invocations);
    EXPECT_EQ(req.computation_time(), computation_time);
    EXPECT_EQ(req.last_invocation_timestamp(), timestamp);
    EXPECT_EQ(req.type(), MessageType::DATAPLANE_METRICS);
  }
}

TEST(Messages, DataPlaneMetricsMsgIncorrect)
{
  {
    DataPlaneMetricsData req;
    EXPECT_THROW(req.invocations(-5), praas::common::InvalidArgument);
  }

  {
    DataPlaneMetricsData req;
    EXPECT_THROW(req.computation_time(-5), praas::common::InvalidArgument);
  }
}

TEST(Messages, DataPlaneMetricsMsgParse)
{
  int32_t invocations{0};
  int32_t computation_time{0};
  uint64_t timestamp = 100;

  DataPlaneMetricsData req;
  req.invocations(invocations);
  req.computation_time(computation_time);
  req.last_invocation_timestamp(timestamp);

  auto parsed = MessageParser::parse(req.ptr());

  EXPECT_TRUE(std::holds_alternative<DataPlaneMetricsPtr>(parsed));

  EXPECT_TRUE(std::visit(
      overloaded{
          [](DataPlaneMetricsPtr&) { return true; },
          [](auto&) { return false; }},
      parsed
  ));
}

TEST(Messages, ProcessClosureMsg)
{
  ProcessClosureData req;

  EXPECT_EQ(req.type(), MessageType::PROCESS_CLOSURE);
}

TEST(Messages, ProcessClosureMsgParse)
{
  ProcessClosureData req;

  auto parsed = MessageParser::parse(req.ptr());

  EXPECT_TRUE(std::holds_alternative<ProcessClosurePtr>(parsed));

  EXPECT_TRUE(std::visit(
      overloaded{
          [](ProcessClosurePtr&) { return true; },
          [](auto&) { return false; }},
      parsed
  ));
}

TEST(Messages, PutRequestMsg)
{
  {
    std::string process_id{"invoc-id-42"};
    std::string fname{"test-name"};
    int32_t payload_size{32};

    PutMessageData req;
    req.process_id(process_id);
    req.name(fname);
    req.total_length(payload_size);

    EXPECT_EQ(req.process_id(), process_id);
    EXPECT_EQ(req.total_length(), payload_size);
    EXPECT_EQ(req.name(), fname);
    EXPECT_EQ(req.type(), MessageType::PUT_MESSAGE);
  }

  {
    std::string process_id(MessageConfig::NAME_LENGTH, 't');
    std::string fname(MessageConfig::NAME_LENGTH, 'a');
    int32_t payload_size{0};

    PutMessageData req;
    req.process_id(process_id);
    req.name(fname);
    req.total_length(payload_size);

    EXPECT_EQ(req.process_id(), process_id);
    EXPECT_EQ(req.total_length(), payload_size);
    EXPECT_EQ(req.name(), fname);
    EXPECT_EQ(req.type(), MessageType::PUT_MESSAGE);
  }
}

TEST(Messages, PutRequestMsgIncorrect)
{
  {
    std::string proc_id(MessageConfig::NAME_LENGTH + 1, 't');

    PutMessageData req;
    EXPECT_THROW(req.process_id(proc_id), praas::common::InvalidArgument);
  }

  {
    std::string fname(MessageConfig::NAME_LENGTH + 1, 'c');

    PutMessageData req;
    EXPECT_THROW(req.name(fname), praas::common::InvalidArgument);
  }
}

TEST(Messages, PutRequestMsgParse)
{
  std::string process_id{"invoc-id-42"};
  std::string fname{"test-name"};
  int32_t payload_size{32};

  PutMessageData req;
  req.process_id(process_id);
  req.name(fname);
  req.total_length(payload_size);

  auto parsed = MessageParser::parse(req.ptr());

  EXPECT_TRUE(std::holds_alternative<PutMessagePtr>(parsed));

  EXPECT_TRUE(std::visit(
      overloaded{
          [=](PutMessagePtr& req) {
            EXPECT_EQ(req.process_id(), process_id);
            EXPECT_EQ(req.total_length(), payload_size);
            EXPECT_EQ(req.name(), fname);
            return true;
          },
          [](auto&) { return false; }},
      parsed
  ));
}
