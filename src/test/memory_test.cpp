#include <gtest/gtest.h>

#include "mock_mega65.h"

namespace m65dap::test {

TEST_F(Mega65Fixture, MemoryReadTest)
{
  conn.write("M1000\n");

  auto line = conn.read_line();
  EXPECT_FALSE(line.second);
  EXPECT_EQ(line.first, "M1000");
  int addr = 0x1000;

  for (int i{0}; i < 16; ++i) {
    line = conn.read_line();
    EXPECT_FALSE(line.second);
    std::string expected_line = fmt::format(":{:08X}:00000000000000000000000000000000", addr);
    EXPECT_EQ(line.first, expected_line);
    addr += 16;
  }
}

}  // namespace m65dap::test
