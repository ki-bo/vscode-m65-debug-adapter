#include "connection_proxy.h"

m65dap::test::ConnectionProxy::ConnectionProxy(std::string_view hw_device) : conn_hw_(hw_device) {}

void m65dap::test::ConnectionProxy::write(std::span<const char> buffer)
{
  conn_hw_.write(buffer);
  conn_mock_.write(buffer);
  std::cerr << fmt::format("-> \"{}\"\n", buffer);
}

auto m65dap::test::ConnectionProxy::read(int bytes_to_read, int timeout_ms) -> std::string
{
  auto result_hw = conn_hw_.read(bytes_to_read, timeout_ms);
  auto result_mock = conn_mock_.read(bytes_to_read, timeout_ms);

  std::cerr << fmt::format("H<- \"{}\"\n", result_hw);
  std::cerr << fmt::format("M<- \"{}\"\n", result_mock);

  if (result_hw != result_mock) {
    throw std::runtime_error(
        fmt::format("Mock inconsistency:\n  HW  : \"{}\"\n  MOCK: \"{}\"\n", result_hw, result_mock));
  }

  return result_hw;
}
