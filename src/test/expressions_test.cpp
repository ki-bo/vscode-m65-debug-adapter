#include <gtest/gtest.h>

#include "m65_debugger.h"
#include "mock_mega65.h"

namespace m65dap::test {

struct ExpressionsFixture : public ::testing::Test, public M65Debugger::EventHandlerInterface {
  ExpressionsFixture() : debugger(std::make_unique<mock::MockMega65>(), this)
  {
    debugger.set_target("data/test.prg");
    debugger.pause();
  }
  M65Debugger debugger;
};

TEST_F(ExpressionsFixture, DirectAddressByteFormat)
{
  auto eval_result = debugger.evaluate_expression("$2001", true);
  EXPECT_EQ(eval_result.address, 0x2001);
  EXPECT_EQ(eval_result.result_string, "09");

  eval_result = debugger.evaluate_expression("$2001", true);
  EXPECT_EQ(eval_result.address, 0x2001);
  EXPECT_EQ(eval_result.result_string, "09");
}

}  // namespace m65dap::test
