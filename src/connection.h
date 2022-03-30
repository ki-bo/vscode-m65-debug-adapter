#pragma once

class Connection
{
public:
  virtual ~Connection() = default;

  virtual void write(std::span<const char> buffer) = 0;

  virtual auto read_line(int timeout_ms = 1000) -> std::pair<std::string, bool> = 0;
  virtual auto read(int bytes_to_read, int timeout_ms = 1000) -> std::string = 0;
  virtual void flush_rx_buffers() = 0;
};
