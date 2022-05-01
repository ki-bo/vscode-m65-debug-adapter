#pragma once

#pragma once

#include "connection.h"
#include "mock_mega65.h"
#include "serial_connection.h"
#include "unix_serial_connection.h"

namespace m65dap::test {

class ConnectionProxy : public Connection {
  std::unique_ptr<Connection> conn_hw_;
  std::unique_ptr<mock::MockMega65> conn_mock_;
  bool is_xemu_{false};

 public:
  ConnectionProxy(std::string_view hw_device);
  bool connected_with_xemu() const { return is_xemu_; }

  void write(std::span<const char> buffer) override final;
  auto read(int bytes_to_read, int timeout_ms = 1000) -> std::string override final;
};

}  // namespace m65dap::test
