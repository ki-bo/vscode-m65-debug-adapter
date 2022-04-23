#pragma once

namespace m65dap {

class Connection {
 public:
  virtual ~Connection() = default;

  virtual void write(std::span<const char> buffer) = 0;

  virtual auto read(int bytes_to_read, int timeout_ms = 1000) -> std::string = 0;
};

}  // namespace m65dap
