#include <gtest/gtest.h>

#include "m65_debugger.h"
#include "mock_mega65.h"

namespace m65dap::test {

struct ExpressionsFixture : public ::testing::Test, public M65Debugger::EventHandlerInterface {
  ExpressionsFixture(bool is_xemu) :
      debugger(std::make_unique<mock::MockMega65>(is_xemu), this, nullptr /*logger*/, is_xemu)
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

struct M65ExpressionsFixture : public ExpressionsFixture {
  M65ExpressionsFixture() : ExpressionsFixture(false) {}
};

struct XemuExpressionsFixture : public ExpressionsFixture {
  XemuExpressionsFixture() : ExpressionsFixture(true) {}
};

TEST_F(M65ExpressionsFixture, DirectAddressByteFormat)
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

TEST_F(XemuExpressionsFixture, DirectAddressByteFormat)
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

TEST_F(M65ExpressionsFixture, DirectAddressWordFormat)
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

TEST_F(XemuExpressionsFixture, DirectAddressWordFormat)
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

TEST_F(M65ExpressionsFixture, DirectAddressQuadFormat)
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

TEST_F(XemuExpressionsFixture, DirectAddressQuadFormat)
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
