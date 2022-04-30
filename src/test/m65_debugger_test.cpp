#include "m65_debugger.h"

#include <gtest/gtest.h>

#include "mock_mega65.h"

namespace m65dap::test {

struct DebuggerFixture : public ::testing::Test, public M65Debugger::EventHandlerInterface {
  DebuggerFixture() : debugger(std::make_unique<mock::MockMega65>(), this) {}
  M65Debugger debugger;
};

TEST(DebuggerSuite, CreateAndDestroyDebugger)
{
  class EventHandler : public M65Debugger::EventHandlerInterface {
  };
  EventHandler dummy_handler;

  auto mock_mega65{std::make_unique<mock::MockMega65>()};
  auto debugger{std::make_unique<M65Debugger>(std::move(mock_mega65), &dummy_handler)};
  debugger.reset();
}

TEST_F(DebuggerFixture, SetTarget) { debugger.set_target("data/test.prg"); }

TEST_F(DebuggerFixture, SetBreakpoint)
{
  auto cp = std::filesystem::current_path();
  debugger.set_target("data/test.prg");
  std::filesystem::path src_path{"data/test_main.asm"};
  debugger.set_breakpoint(src_path, 79);
}

}  // namespace m65dap::test
