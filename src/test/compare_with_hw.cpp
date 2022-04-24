#include <gtest/gtest.h>

#include <memory>

#include "connection_proxy.h"
#include "m65_debugger.h"
#include "test/mock_mega65.h"
#include "test_common.h"

namespace m65dap::test {

struct TwoConnectionsFixture : public ::testing::Test, public M65Debugger::EventHandlerInterface {
 protected:
  TwoConnectionsFixture()
  {
    try {
      debugger = std::make_unique<M65Debugger>(std::make_unique<ConnectionProxy>("COM4"), this, true, false);
    }
    catch (...) {
      debugger = std::make_unique<M65Debugger>(std::make_unique<mock::MockMega65>(), this, true, false);
    }
  }

  std::unique_ptr<M65Debugger> debugger;
};

TEST_F(TwoConnectionsFixture, SetTarget) { debugger->set_target("data/test.prg"); }

TEST_F(TwoConnectionsFixture, Breakpoint)
{
  debugger->set_target("data/test.prg");
  debugger->set_breakpoint("data/test_main.asm", 25);
}

}  // namespace m65dap::test
