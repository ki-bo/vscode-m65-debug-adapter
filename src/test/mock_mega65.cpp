#include "mock_mega65.h"

namespace m65dap::test::mock {

MockMega65::MockMega65() : memory_(384 * 1024) {}

void MockMega65::write(std::span<const char> buffer)
{
  if (buffer.size() == 0) {
    return;
  }

  const std::string crlf{"\r\n"};
  auto it = std::find_first_of(buffer.begin(), buffer.end(), crlf.begin(), crlf.end());
  throw_if<std::invalid_argument>(it == buffer.end(), "No eol char found");
  
  if (it == buffer.begin()) {
    return;
  }

  std::string_view input_str(buffer.data(), it - buffer.begin());
  if (input_str == "?") {
    lines_.emplace("MEGA65 Serial Monitor");
    lines_.emplace(".");
  }
  if (parse_memory_cmd(input_str)) {
    return;
  }
}

auto MockMega65::read_line(int timeout_ms) -> std::pair<std::string, bool>
{
  if (lines_.empty()) {
    return {"", true};
  }

  auto line{std::move(lines_.front())};
  lines_.pop();
  return {line, false};
}

auto MockMega65::read(int bytes_to_read, int timeout_ms) -> std::string
{
  return {};
}

void MockMega65::flush_rx_buffers() 
{
}

auto MockMega65::parse_memory_cmd(std::string_view line) -> bool
{
  static const std::regex r(R"(^\s*([mM])\s*([0-9a-fA-F]+)\s*$)");

  std::cmatch match;
  if (!std::regex_search(line.cbegin(), line.cend(), match, r)) {
    return false;
  }

  const auto& cmd_match{match[1]};
  const auto& address_match{match[2]};
  int address = str_to_int(address_match.str(), 16);
  int num_lines = *cmd_match.first == 'm' ? 1 : 32;
  throw_if<std::out_of_range>(
      address + num_lines * 16 >= memory_.size(),
      fmt::format("Memory request at address {} with size {} out of range", address, num_lines * 16));

  lines_.emplace(std::string(line));
  for (int i{0}; i < num_lines; ++i) {
    auto mem_range{std::span<char>(memory_).subspan(address, 16)};
    lines_.emplace(fmt::format(":{:08X}:{:02X}", address, fmt::join(mem_range, "")));
    address += 16;
  }
  return true;
}


}  // namespace m65dap::test::mock
