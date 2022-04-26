#pragma once

#include <gtest/gtest.h>

#include "connection.h"

namespace m65dap::test::mock {

class MockMega65 : public Connection {
  std::string output_buffer_;
  std::vector<uint8_t> memory_;
  bool trace_mode_{false};
  bool breakpoint_set_{false};
  int load_addr_{0};
  int load_remaining_bytes_{0};

 public:
  MockMega65();

  void write(std::span<const char> buffer) override final;
  auto read(int bytes_to_read, int timeout_ms = 1000) -> std::string override final;
  auto read_line(int timeout_ms = 1000) -> std::pair<std::string, bool>;
  void flush_rx_buffers();

 private:
  auto process_load_bytes(std::span<const char> buffer) -> std::span<const char>;
  auto parse_help_cmd(std::string_view line) -> bool;
  auto parse_memory_cmd(std::string_view line) -> bool;
  auto parse_trace_cmd(std::string_view line) -> bool;
  auto parse_reset_cmd(std::string_view line) -> bool;
  auto parse_load_cmd(std::string_view line) -> bool;
  auto parse_break_cmd(std::string_view line) -> bool;
  auto parse_store_cmd(std::string_view line) -> bool;
};

}  // namespace m65dap::test::mock

namespace m65dap::test {

struct Mega65Fixture : public ::testing::Test {
 protected:
  mock::MockMega65 conn;
};

}  // namespace m65dap::test
