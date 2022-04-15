#include <gtest/gtest.h>

#include "mock_mega65.h"

namespace m65dap::test {

TEST_F(Mega65Fixture, ReadTimeout)
{
  auto line = conn.read_line();
  EXPECT_TRUE(line.second);
  EXPECT_TRUE(line.first.empty());
}

TEST_F(Mega65Fixture, InvalidWrite)
{
  EXPECT_THROW(conn.write(""), std::invalid_argument);
  EXPECT_THROW(conn.write("abc"), std::invalid_argument);
  EXPECT_NO_THROW(conn.write("?\n"));
  EXPECT_NO_THROW(conn.write("?\r\n"));
}

TEST_F(Mega65Fixture, ReadHelpResponse)
{
  conn.write("?\n");
  auto line = conn.read_line();
  EXPECT_FALSE(line.second);
  EXPECT_EQ(line.first, "MEGA65 Serial Monitor");

  line = conn.read_line();
  EXPECT_FALSE(line.second);
  EXPECT_EQ(line.first, ".");

  line = conn.read_line();
  EXPECT_TRUE(line.second);
  EXPECT_TRUE(line.first.empty());
}

}  // namespace m65dap::test
