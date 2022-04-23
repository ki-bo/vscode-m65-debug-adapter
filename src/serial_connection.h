#pragma once
#include <serial/serial.h>

#include "connection.h"


namespace m65dap {

class SerialConnection : public Connection {
  serial::Serial device_;

 public:
  SerialConnection(std::string_view device);
  void write(std::span<const char> buffer) final;
  auto read(int bytes_to_read, int timeout_ms = 1000) -> std::string final;
};

}  // namespace m65dap
