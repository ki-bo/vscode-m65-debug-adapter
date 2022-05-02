#include <gtest/gtest.h>

#include "mock_mega65_fixture.h"
#include "mock_xemu_fixture.h"
#include "test_common.h"

namespace m65dap::test {

TEST_F(MockMega65Fixture, ReadTimeout)
{
  auto line = conn.read_line();
  EXPECT_TRUE(line.second);
  EXPECT_TRUE(line.first.empty());
}

TEST_F(MockXemuFixture, ReadTimeout)
{
  auto line = conn.read_line();
  EXPECT_TRUE(line.second);
  EXPECT_TRUE(line.first.empty());
}

TEST_F(MockMega65Fixture, InvalidWrite)
{
  EXPECT_THROW(conn.write(""), std::invalid_argument);
  EXPECT_THROW(conn.write("abc"), std::invalid_argument);
  EXPECT_NO_THROW(conn.write("?\n"));
  EXPECT_NO_THROW(conn.write("?\r\n"));
}

TEST_F(MockXemuFixture, InvalidWrite)
{
  EXPECT_THROW(conn.write(""), std::invalid_argument);
  EXPECT_THROW(conn.write("abc"), std::invalid_argument);
  EXPECT_NO_THROW(conn.write("?\n"));
  EXPECT_NO_THROW(conn.write("?\r\n"));
}

TEST_F(MockMega65Fixture, ReadHelpResponse)
{
  std::string cmd = "?";
  const std::vector<std::string> expected{
      {"?"}, {"MEGA65 Serial Monitor"}, {"build GIT: development,20220305.00,ee4f29d"}, {}, {"."}};
  test_command(conn, cmd, expected);
}

TEST_F(MockXemuFixture, ReadHelpResponse)
{
  std::string cmd = "?";
  const std::vector<std::string> expected{{"?"},
                                          {"Xemu/MEGA65 Serial Monitor"},
                                          {"Warning: not 100% compatible with UART monitor of a *real* MEGA65 ..."},
                                          {"."}};
  test_command(conn, cmd, expected);
}

TEST_F(MockMega65Fixture, ReadHelpResponseWithOptionalNumber)
{
  std::string cmd = "?77";
  const std::vector<std::string> expected{
      {"?77"}, {"MEGA65 Serial Monitor"}, {"build GIT: development,20220305.00,ee4f29d"}, {}, {"."}};
  test_command(conn, cmd, expected);
}

}  // namespace m65dap::test
