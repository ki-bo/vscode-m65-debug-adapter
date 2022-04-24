#pragma once

#pragma once

#include "connection.h"
#include "mock_mega65.h"
#include "serial_connection.h"

namespace m65dap::test {

class ConnectionProxy : public Connection {
  SerialConnection conn_hw_;
  mock::MockMega65 conn_mock_;

 public:
  ConnectionProxy(std::string_view hw_device);

  void write(std::span<const char> buffer) override final;
  auto read(int bytes_to_read, int timeout_ms = 1000) -> std::string override final;
};

}  // namespace m65dap::test
