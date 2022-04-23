#pragma once

#include "connection.h"

namespace m65dap {

class UnixConnection : public Connection {
 protected:
  int fd_;

 public:
  void write(std::span<const char> buffer) override;

  auto read(int bytes_to_read, int timeout_ms = 1000) -> std::string override;
};

}  // namespace m65dap
