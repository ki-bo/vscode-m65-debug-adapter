#include "serial_connection.h"

#include <serial/serial.h>

namespace m65dap {

SerialConnection::SerialConnection(std::string_view device) : device_(std::string(device), 2000000) {}

void SerialConnection::write(std::span<const char> buffer)
{
  device_.write(reinterpret_cast<const std::uint8_t*>(buffer.data()), buffer.size());
}

auto SerialConnection::read(int bytes_to_read, int timeout_ms) -> std::string
{
  static serial::Timeout t(serial::Timeout::simpleTimeout(0));
  t.read_timeout_constant = timeout_ms;
  device_.setTimeout(t);
  return device_.read(bytes_to_read);
}

}  // namespace m65dap
