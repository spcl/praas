
#include <praas/common/exceptions.hpp>
#include <praas/process/runtime/functions.hpp>

#include <sstream>

#include <gtest/gtest.h>

using namespace praas::process::runtime::functions;

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

  Functions functions;
  functions.initialize(stream, Language::CPP);

  auto ptr = functions.get_trigger("test");
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(ptr->type(), Trigger::Type::DIRECT);

  auto func_ptr = functions.get_function("test");
  ASSERT_NE(func_ptr, nullptr);
  EXPECT_EQ(func_ptr->function_name, "test");
  EXPECT_EQ(func_ptr->module_name, "libtest.so");
}

