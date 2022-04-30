#include "connection_proxy.h"

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

m65dap::test::ConnectionProxy::ConnectionProxy(std::string_view hw_device) : conn_hw_(hw_device) {}

void m65dap::test::ConnectionProxy::write(std::span<const char> buffer)
{
  conn_hw_.write(buffer);
  conn_mock_.write(buffer);
  std::cerr << format_string(buffer);
}

auto m65dap::test::ConnectionProxy::read(int bytes_to_read, int timeout_ms) -> std::string
{
  auto result_hw = conn_hw_.read(bytes_to_read, timeout_ms);
  auto result_mock = conn_mock_.read(result_hw.length(), timeout_ms);

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
