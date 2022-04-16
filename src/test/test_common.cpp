#include "test_common.h"

#include <gtest/gtest.h>

namespace m65dap::test {

void test_command(mock::MockMega65& conn, std::string_view cmd, const std::vector<std::string>& expected_lines)
{
  conn.write(std::string(cmd) + '\n');
  for (const auto& expected_line : expected_lines) {
    auto line = conn.read_line();
    ASSERT_FALSE(line.second);
    ASSERT_EQ(line.first, expected_line);
  }

  auto line2 = conn.read_line();
  ASSERT_TRUE(line2.second);
  ASSERT_TRUE(line2.first.empty());
}

}  // namespace m65dap::test
