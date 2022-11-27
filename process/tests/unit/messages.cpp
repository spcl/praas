
#include <praas/process/runtime/ipc/messages.hpp>

#include <praas/common/exceptions.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace praas::process::runtime::ipc;

template <typename A, typename B>
struct TypeDefinitions {
  using Msg = A;
  using MsgParsed = B;
};

template <class T>
class IPCMessagesTest : public testing::Test {
};

using Implementations = ::testing::Types<
    TypeDefinitions<GetRequest, GetRequestParsed>, TypeDefinitions<PutRequest, PutRequestParsed>>;

TYPED_TEST_SUITE(IPCMessagesTest, Implementations);

TYPED_TEST(IPCMessagesTest, Message)
{
  using MsgType = typename TypeParam::Msg;
  {
    std::string process_name{"test-name"};
    std::string object_name{"test-id"};
    int data_len = 42;

    MsgType req;
    req.process_id(process_name);
    req.name(object_name);
    req.data_len(data_len);

    EXPECT_EQ(req.process_id(), process_name);
    EXPECT_EQ(req.name(), object_name);
    EXPECT_EQ(req.data_len(), data_len);
    EXPECT_EQ(req.type(), MsgType::TYPE);
  }

  {
    std::string process_name(MsgType::NAME_LENGTH, 't');
    std::string object_name(MsgType::NAME_LENGTH, 's');
    int data_len = 1;

    MsgType req;
    req.process_id(process_name);
    req.name(object_name);
    req.data_len(data_len);

    EXPECT_EQ(req.process_id(), process_name);
    EXPECT_EQ(req.name(), object_name);
    EXPECT_EQ(req.data_len(), data_len);
    EXPECT_EQ(req.type(), MsgType::TYPE);
  }
}

TYPED_TEST(IPCMessagesTest, MessageIncorrect)
{
  using MsgType = typename TypeParam::Msg;

  std::string process_name(MsgType::NAME_LENGTH + 1, 't');

  MsgType req;
  EXPECT_THROW(req.process_id(process_name), praas::common::InvalidArgument);
  EXPECT_THROW(req.name(process_name), praas::common::InvalidArgument);
}

TYPED_TEST(IPCMessagesTest, MessageParse)
{
  using MsgType = typename TypeParam::Msg;
  using MsgTypeParsed = typename TypeParam::MsgParsed;

  std::string process_name{"test-name"};
  std::string object_name{"test-id"};
  int data_len = 42;

  MsgType req;
  req.process_id(process_name);
  req.name(object_name);
  req.data_len(data_len);

  Message& msg = *static_cast<Message*>(&req);
  auto parsed = msg.parse();

  EXPECT_TRUE(std::holds_alternative<MsgTypeParsed>(parsed));

  EXPECT_TRUE(std::visit(
      overloaded{
          [=](MsgTypeParsed& req) {
            EXPECT_EQ(req.process_id(), process_name);
            EXPECT_EQ(req.name(), object_name);
            EXPECT_EQ(req.data_len(), data_len);

            return true;
          },
          [](auto&) { return false; }},
      parsed
  ));
}

TEST(IPCMessagesInvocTest, InvocMessage)
{

  {
    std::string invoc_id{"test-name"};
    std::string func_name{"test-id"};
    std::array<int, 1> buffers = {5};

    InvocationRequest req;
    req.invocation_id(invoc_id);
    req.function_name(func_name);
    req.buffers(buffers.begin(), buffers.end());
    req.total_length(10);

    EXPECT_EQ(req.type(), Message::Type::INVOCATION_REQUEST);
    EXPECT_EQ(req.invocation_id(), invoc_id);
    EXPECT_EQ(req.function_name(), func_name);
    EXPECT_EQ(req.buffers(), buffers.size());
    EXPECT_EQ(req.total_length(), 10);
    EXPECT_THAT(
        buffers,
        testing::ElementsAreArray(req.buffers_lengths(), req.buffers())
    );
  }

  {
    std::string invoc_id(GetRequest::ID_LENGTH, 't');
    std::string func_name(GetRequest::NAME_LENGTH, 's');
    std::array<int, 5> buffers = {5, 42, 0, 1, 32};

    InvocationRequest req;
    req.invocation_id(invoc_id);
    req.function_name(func_name);
    req.buffers(buffers.begin(), buffers.end());
    req.total_length(42);

    EXPECT_EQ(req.type(), Message::Type::INVOCATION_REQUEST);
    EXPECT_EQ(req.invocation_id(), invoc_id);
    EXPECT_EQ(req.function_name(), func_name);
    EXPECT_EQ(req.buffers(), buffers.size());
    EXPECT_EQ(req.total_length(), 42);
    EXPECT_THAT(
        buffers,
        testing::ElementsAreArray(req.buffers_lengths(), req.buffers())
    );
  }
}

TEST(IPCMessagesInvocTest, InvocMessageIncorrect)
{
  std::string invoc_id(GetRequest::ID_LENGTH + 1, 't');
  std::string func_name(GetRequest::NAME_LENGTH + 2, 's');
  std::array<int, 33> buffers{};

  InvocationRequest req;
  EXPECT_THROW(req.invocation_id(invoc_id), praas::common::InvalidArgument);
  EXPECT_THROW(req.function_name(func_name), praas::common::InvalidArgument);
  EXPECT_THROW(req.buffers(buffers.begin(), buffers.end()), praas::common::InvalidArgument);
}

TEST(IPCMessagesInvocTest, InvocMessageParse)
{
  std::string invoc_id{"test-name"};
  std::string func_name{"test-id"};
  std::array<int, 1> buffers = {5};

  InvocationRequest req;
  req.invocation_id(invoc_id);
  req.function_name(func_name);
  req.buffers(buffers.begin(), buffers.end());
  req.total_length(42);

  Message& msg = *static_cast<Message*>(&req);
  EXPECT_EQ(msg.total_length(), 42);

  auto parsed = msg.parse();

  EXPECT_TRUE(std::holds_alternative<InvocationRequestParsed>(parsed));

  EXPECT_TRUE(std::visit(
      overloaded{
          [=](InvocationRequestParsed& req) {

            EXPECT_EQ(req.invocation_id(), invoc_id);
            EXPECT_EQ(req.function_name(), func_name);
            EXPECT_EQ(req.buffers(), buffers.size());
            EXPECT_THAT(
                buffers,
                testing::ElementsAreArray(req.buffers_lengths(), req.buffers())
            );

            return true;
          },
          [](auto&) { return false; }},
      parsed
  ));
}

TEST(IPCMessagesInvocTest, InvocResultMessage)
{

  {
    std::string invoc_id{"test-name"};
    int buffer_length = 42;

    InvocationResult req;
    req.invocation_id(invoc_id);
    req.buffer_length(buffer_length);

    EXPECT_EQ(req.type(), Message::Type::INVOCATION_RESULT);
    EXPECT_EQ(req.invocation_id(), invoc_id);
    EXPECT_EQ(req.buffer_length(), buffer_length);
  }

  {
    std::string invoc_id(GetRequest::ID_LENGTH, 't');
    int buffer_length = 1;

    InvocationResult req;
    req.invocation_id(invoc_id);
    req.buffer_length(buffer_length);

    EXPECT_EQ(req.type(), Message::Type::INVOCATION_RESULT);
    EXPECT_EQ(req.invocation_id(), invoc_id);
    EXPECT_EQ(req.buffer_length(), buffer_length);
  }
}

TEST(IPCMessagesInvocTest, InvocResultIncorrect)
{
  std::string invoc_id(GetRequest::ID_LENGTH + 3, 't');

  InvocationResult req;
  EXPECT_THROW(req.invocation_id(invoc_id), praas::common::InvalidArgument);
}

TEST(IPCMessagesInvocTest, InvocResultParse)
{
  std::string invoc_id{"test-name"};
  int buffer_length = 42;

  InvocationResult req;
  req.invocation_id(invoc_id);
  req.buffer_length(buffer_length);

  Message& msg = *static_cast<Message*>(&req);
  auto parsed = msg.parse();

  EXPECT_TRUE(std::holds_alternative<InvocationResultParsed>(parsed));

  EXPECT_TRUE(std::visit(
      overloaded{
          [=](InvocationResultParsed& req) {

            EXPECT_EQ(req.invocation_id(), invoc_id);
            EXPECT_EQ(req.buffer_length(), buffer_length);

            return true;
          },
          [](auto&) { return false; }},
      parsed
  ));
}
