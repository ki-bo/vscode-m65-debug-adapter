#include "connection_proxy.h"

#include "unix_domain_socket_connection.h"

namespace {

auto format_string(std::span<const char> buffer) -> std::string
{
  if (m65dap::is_ascii(buffer)) {
    std::string result(buffer.data(), buffer.size());
    m65dap::replace_all(result, "\n", "\\n");
    m65dap::replace_all(result, "\r", "\\r");
    return fmt::format("-> \"{}\"\n", result);
  }
  else {
    return fmt::format("-> binary data (size {0:}/${0:X} bytes)\n", buffer.size_bytes());
  }
}
}  // namespace

namespace m65dap::test {

ConnectionProxy::ConnectionProxy(std::string_view hw_device)
{
  if (hw_device.starts_with("unix#")) {
    std::string_view socket_path = hw_device.substr(5);
    conn_hw_ = std::make_unique<UnixDomainSocketConnection>(socket_path);
    is_xemu_ = true;
  }
  else {
#ifdef _POSIX_VERSION
    conn_hw_ = std::make_unique<UnixSerialConnection>(hw_device);
#else
    conn_hw_ = std::make_unique<SerialConnection>(hw_device);
#endif
  }

  conn_mock_ = std::make_unique<mock::MockMega65>(is_xemu_);

  // Flush rx buffer
  conn_hw_->read(65536, 100);
}

void ConnectionProxy::write(std::span<const char> buffer)
{
  conn_hw_->write(buffer);
  conn_mock_->write(buffer);
  std::cerr << format_string(buffer);
}

auto ConnectionProxy::read(int bytes_to_read, int timeout_ms) -> std::string
{
  auto result_hw = conn_hw_->read(bytes_to_read, timeout_ms);
  auto result_mock = conn_mock_->read(result_hw.length(), timeout_ms);

  if (result_hw.empty()) {
    return {};
  }

  std::cerr << fmt::format("H<- \"{}\"\n", result_hw);
  std::cerr << fmt::format("M<- \"{}\"\n", result_mock);

  if (result_hw != result_mock) {
    throw std::runtime_error(
        fmt::format("Mock inconsistency:\n  HW  : \"{}\"\n  MOCK: \"{}\"\n", result_hw, result_mock));
  }

  return result_hw;
}

}  // namespace m65dap::test
