#include <gtest/gtest.h>

#include "mock_mega65.h"

namespace m65dap::test {

TEST_F(Mega65Fixture, MemoryRead)
{
  conn.write("M1000\n");

  auto line = conn.read_line();
  EXPECT_FALSE(line.second);
  EXPECT_EQ(line.first, "M1000");
  int addr = 0x1000;

  for (int i{0}; i < 32; ++i) {
    line = conn.read_line();
    EXPECT_FALSE(line.second);
    std::string expected_line = fmt::format(":{:08X}:00000000000000000000000000000000", addr);
    EXPECT_EQ(line.first, expected_line);
    addr += 16;
  }

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
  std::vector<char> data(512);
  std::iota(data.begin(), data.end() - 12, 1);
  int load_address = 0x2001;
  const int end_address_plus_one = load_address + data.size();
  auto cmd = fmt::format("l{:X} {:X}\n", load_address, end_address_plus_one);

  conn.write(cmd);
  conn.write(data);
  auto line = conn.read_line();
  EXPECT_FALSE(line.second);
  EXPECT_EQ(line.first, "l2001 2201");
  ASSERT_EQ(conn.read(3), "\r\n.");

  cmd = fmt::format("M{:X}\n", load_address);
  conn.write(cmd);
  line = conn.read_line();
  EXPECT_FALSE(line.second);
  EXPECT_EQ(line.first, fmt::format("M{:X}", load_address));

  for (int i{0}; i < 32; ++i) {
    line = conn.read_line();
    EXPECT_FALSE(line.second);
    std::span<std::uint8_t> data_span(reinterpret_cast<std::uint8_t*>(&data.at(i * 16)), 16);
    std::string expected_line = fmt::format(":{:08X}:{:02X}", load_address, fmt::join(data_span, ""));
    EXPECT_EQ(line.first, expected_line);
    load_address += 16;
  }
}

TEST_F(Mega65Fixture, LoadDataIncludingNextCmd)
{
  int load_address = 0x2001;
  auto mem_cmd = fmt::format("M{:X}\n", load_address);

  std::vector<char> data(512);
  int data_size = data.size() - mem_cmd.length();
  std::iota(data.begin(), data.begin() + data_size, 1);
  std::copy(mem_cmd.begin(), mem_cmd.end(), &data.at(data_size));

  const int end_address_plus_one = load_address + data_size;
  auto cmd = fmt::format("l{:X} {:X}\n", load_address, end_address_plus_one);

  conn.write(cmd);
  conn.write(data);
  auto line = conn.read_line();
  EXPECT_FALSE(line.second);
  EXPECT_EQ(line.first, "l2001 21FB");
  ASSERT_EQ(conn.read(3), "\r\n.");

  line = conn.read_line();
  EXPECT_FALSE(line.second);
  EXPECT_EQ(line.first, fmt::format("M{:X}", load_address));

  std::fill(data.begin() + data_size, data.end(), 0);
  for (int i{0}; i < 32; ++i) {
    line = conn.read_line();
    EXPECT_FALSE(line.second);
    std::span<std::uint8_t> data_span(reinterpret_cast<std::uint8_t*>(&data.at(i * 16)), 16);
    std::string expected_line = fmt::format(":{:08X}:{:02X}", load_address, fmt::join(data_span, ""));
    EXPECT_EQ(line.first, expected_line);
    load_address += 16;
  }
}

}  // namespace m65dap::test
