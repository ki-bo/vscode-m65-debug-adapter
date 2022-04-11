#pragma once

#include "connection.h"

namespace m65dap
{

class UnixConnection : public Connection
{
  std::string buffer_;

protected:
  int fd_;
  bool last_line_was_empty_ {false};

public:

  void write(std::span<const char> buffer) override;

  auto read_line(int timeout_ms = 1000) -> std::pair<std::string, bool> override;
  auto read(int bytes_to_read, int timeout_ms = 1000) -> std::string override;

  void flush_rx_buffers() override;

};

} // namespace
