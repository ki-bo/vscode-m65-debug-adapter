#include <gtest/gtest.h>

#include "mock_mega65_fixture.h"
#include "test_common.h"

namespace m65dap::test {

TEST_F(MockMega65Fixture, TraceOff)
{
  std::string cmd = "t0";
  const std::vector<std::string> expected{{"t0"}, {""}, {"."}};
  test_command(conn, cmd, expected);
}

TEST_F(MockMega65Fixture, TraceOn)
{
  std::string cmd = "t1";
  const std::vector<std::string> expected{{"t1"}, {""}, {"."}};
  test_command(conn, cmd, expected);
}

}  // namespace m65dap::test
