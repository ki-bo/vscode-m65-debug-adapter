#include <gtest/gtest.h>

#include "mock_mega65_fixture.h"

namespace m65dap::test {

TEST_F(Mega65Fixture, MemoryRead)
{
  conn.write("M1000\n");

  auto line = conn.read_line();
  EXPECT_FALSE(line.second);
  EXPECT_EQ(line.first, "M1000");

  std::vector<std::string> lines;
  for (int i{0}; i < 16; ++i) {
    line = conn.read_line();
    EXPECT_FALSE(line.second);
    lines.push_back(line.first);
  }

  EXPECT_EQ(lines[0], ":00001000:00000000000000000000000000000000");
  EXPECT_EQ(lines[1], ":00001010:00000000000000000000000000000000");
  EXPECT_EQ(lines[15], ":000010F0:00000000000000000000000000000000");

  line = conn.read_line();
  EXPECT_FALSE(line.second);
  EXPECT_EQ(line.first, "");
  line = conn.read_line();
  EXPECT_FALSE(line.second);
  EXPECT_EQ(line.first, ".");
  line = conn.read_line();
  EXPECT_TRUE(line.second);
  EXPECT_EQ(line.first, "");
}

TEST_F(Mega65Fixture, LoadDataOneShot)
{
  std::vector<char> data(256);
  std::iota(data.begin(), data.end() - 12, 1);
  int load_address = 0x2001;
  const int end_address_plus_one = load_address + data.size();
  auto cmd = fmt::format("l{:X} {:X}\n", load_address, end_address_plus_one);

  conn.write(cmd);
  conn.write(data);
  auto line = conn.read_line();
  EXPECT_FALSE(line.second);
  EXPECT_EQ(line.first, "l2001 2101");
  ASSERT_EQ(conn.read(3), "\r\n.");

  cmd = fmt::format("M{:X}\n", load_address);
  conn.write(cmd);
  line = conn.read_line();
  EXPECT_FALSE(line.second);
  EXPECT_EQ(line.first, fmt::format("M{:X}", load_address));

  std::vector<std::string> lines;
  for (int i{0}; i < 16; ++i) {
    line = conn.read_line();
    EXPECT_FALSE(line.second);
    lines.push_back(line.first);
  }

  EXPECT_EQ(lines[0], ":00002001:0102030405060708090A0B0C0D0E0F10");
  EXPECT_EQ(lines[1], ":00002011:1112131415161718191A1B1C1D1E1F20");
  EXPECT_EQ(lines[15], ":000020F1:F1F2F3F4000000000000000000000000");
}

TEST_F(Mega65Fixture, LoadDataIncludingNextCmd)
{
  int load_address = 0x2001;
  auto mem_cmd = fmt::format("M{:X}\n", load_address);

  std::vector<char> data(256);
  int data_size = data.size() - mem_cmd.length();
  std::iota(data.begin(), data.begin() + data_size, 1);
  std::copy(mem_cmd.begin(), mem_cmd.end(), &data.at(data_size));

  const int end_address_plus_one = load_address + data_size;
  auto cmd = fmt::format("l{:X} {:X}\n", load_address, end_address_plus_one);

  conn.write(cmd);
  conn.write(data);
  auto line = conn.read_line();
  EXPECT_FALSE(line.second);
  EXPECT_EQ(line.first, "l2001 20FB");
  ASSERT_EQ(conn.read(3), "\r\n.");

  line = conn.read_line();
  EXPECT_FALSE(line.second);
  EXPECT_EQ(line.first, fmt::format("M{:X}", load_address));

  std::vector<std::string> lines;
  for (int i{0}; i < 16; ++i) {
    line = conn.read_line();
    EXPECT_FALSE(line.second);
    lines.push_back(line.first);
  }

  EXPECT_EQ(lines[0], ":00002001:0102030405060708090A0B0C0D0E0F10");
  EXPECT_EQ(lines[1], ":00002011:1112131415161718191A1B1C1D1E1F20");
  EXPECT_EQ(lines[15], ":000020F1:F1F2F3F4F5F6F7F8F9FA000000000000");
}

}  // namespace m65dap::test
