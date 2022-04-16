#include <gtest/gtest.h>

#include "mock_mega65.h"
#include "test_common.h"

namespace m65dap::test {

TEST_F(Mega65Fixture, TraceOff)
{
  std::string cmd = "t0";
  const std::vector<std::string> expected{{"t0"}, {"."}};
  test_command(conn, cmd, expected);
}

TEST_F(Mega65Fixture, TraceOn)
{
  std::string cmd = "t1";
  const std::vector<std::string> expected{{"t1"}, {"."}};
  test_command(conn, cmd, expected);
}

}  // namespace m65dap::test