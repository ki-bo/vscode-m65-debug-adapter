#pragma once

#include "connection.h"

class SerialConnection : public Connection
{
  int fd_;
  std::string buffer_;

public:
  SerialConnection(std::string_view port);

  void write(std::span<const char> buffer) override;

  auto read_line(int timeout_ms = 1000) -> std::pair<std::string, bool> override;
  auto read(int bytes_to_read, int timeout_ms = 1000) -> std::string override;

  void flush_rx_buffers() override;
};
