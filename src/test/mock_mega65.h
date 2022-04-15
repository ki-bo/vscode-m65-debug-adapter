#pragma once

#include <gtest/gtest.h>

#include "connection.h"

namespace m65dap::test::mock {

class MockMega65 : public Connection {
  std::queue<std::string> lines_;
  std::vector<char> memory_;

 public:
  MockMega65();

  void write(std::span<const char> buffer) override final;

  auto read_line(int timeout_ms = 1000) -> std::pair<std::string, bool> override final;
  auto read(int bytes_to_read, int timeout_ms = 1000) -> std::string override final;
  void flush_rx_buffers() override final;

 private:
  auto parse_memory_cmd(std::string_view line) -> bool;
};

}  // namespace m65dap::test::mock

namespace m65dap::test {

struct Mega65Fixture : public ::testing::Test {
  mock::MockMega65 conn;
};

}  // namespace m65dap::test
