#include <gtest/gtest.h>

#include "m65_debugger.h"
#include "mock_mega65.h"

namespace m65dap::test {

struct ExpressionsFixture : public ::testing::Test, public M65Debugger::EventHandlerInterface {
  ExpressionsFixture() : debugger(std::make_unique<mock::MockMega65>(), this)
  {
    debugger.set_target("data/test.prg");
    debugger.set_breakpoint("data/test_main.asm", 79);
    debugger.run_target();
    stopped_event_promise.get_future().wait();
  }

  void handle_debugger_stopped(M65Debugger::StoppedReason) { stopped_event_promise.set_value(); }

  M65Debugger debugger;
  std::promise<void> stopped_event_promise;
};

TEST_F(ExpressionsFixture, DirectAddressByteFormat)
{
  auto eval_result = debugger.evaluate_expression("$2001", true);
  EXPECT_EQ(eval_result.address, 0x2001);
  EXPECT_EQ(eval_result.result_string, "09");

  eval_result = debugger.evaluate_expression("$2002", true);
  EXPECT_EQ(eval_result.address, 0x2002);
  EXPECT_EQ(eval_result.result_string, "20");

  eval_result = debugger.evaluate_expression("$2001,2", true);
  EXPECT_EQ(eval_result.address, 0x2001);
  EXPECT_EQ(eval_result.result_string, "09 20");
}

TEST_F(ExpressionsFixture, DirectAddressWordFormat)
{
  auto eval_result = debugger.evaluate_expression("$2001,w", true);
  EXPECT_EQ(eval_result.address, 0x2001);
  EXPECT_EQ(eval_result.result_string, "2009");

  eval_result = debugger.evaluate_expression("$2001,w,2", true);
  EXPECT_EQ(eval_result.address, 0x2001);
  EXPECT_EQ(eval_result.result_string, "2009 0472");

  eval_result = debugger.evaluate_expression("$2001,w,3", true);
  EXPECT_EQ(eval_result.address, 0x2001);
  EXPECT_EQ(eval_result.result_string, "2009 0472 02FE");
}

TEST_F(ExpressionsFixture, DirectAddressQuadFormat)
{
  auto eval_result = debugger.evaluate_expression("$2001,q", true);
  EXPECT_EQ(eval_result.address, 0x2001);
  EXPECT_EQ(eval_result.result_string, "04722009");

  eval_result = debugger.evaluate_expression("$2001,q,2", true);
  EXPECT_EQ(eval_result.address, 0x2001);
  EXPECT_EQ(eval_result.result_string, "04722009 003002FE");

  eval_result = debugger.evaluate_expression("$2001,q,3", true);
  EXPECT_EQ(eval_result.address, 0x2001);
  EXPECT_EQ(eval_result.result_string, "04722009 003002FE 16E22013");
}

}  // namespace m65dap::test
