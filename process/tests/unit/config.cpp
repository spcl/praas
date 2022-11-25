
#include <praas/common/exceptions.hpp>
#include <praas/process/controller/workers.hpp>

#include <sstream>

#include <gtest/gtest.h>

using namespace praas::process;

TEST(ProcessFunctionsConfig, TriggerDirect)
{
  std::string config = R"(
    {
      "functions": {
        "cpp": {
          "test": {
            "code": {
              "module": "libtest.so",
              "function": "test"
            },
            "trigger": {
              "type": "direct",
              "nargs": 1
            }
          }
        }
      }
    }
  )";

  std::stringstream stream{config};

  WorkQueue queue;
  queue.initialize(stream, config::Language::CPP);

  auto ptr = queue.get_trigger("test");
  ASSERT_NE(ptr, nullptr);

  EXPECT_EQ(ptr->type(), Trigger::Type::DIRECT);
}

