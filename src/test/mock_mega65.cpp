#include "mock_mega65.h"

namespace {

const std::string eol_str{"\r\n"};

}
namespace m65dap::test::mock {

MockMega65::MockMega65() : memory_(384 * 1024) {}

void MockMega65::write(std::span<const char> buffer)
{
  if (load_remaining_bytes_ > 0) {
    buffer = process_load_bytes(buffer);
  }

  if (buffer.size() == 0) {
    return;
  }

  auto it = std::find_first_of(buffer.begin(), buffer.end(), eol_str.begin(), eol_str.end());
  throw_if<std::invalid_argument>(it == buffer.end(), "No eol char found");

  if (it == buffer.begin()) {
    return;
  }

  std::string_view input_str(buffer.data(), it - buffer.begin());
  if (parse_help_cmd(input_str)) {
    return;
  }
  if (parse_memory_cmd(input_str)) {
    return;
  }
  if (parse_trace_cmd(input_str)) {
    return;
  }
  if (parse_reset_cmd(input_str)) {
    return;
  }
  if (parse_load_cmd(input_str)) {
    return;
  }
}

auto MockMega65::read_line(int timeout_ms) -> std::pair<std::string, bool>
{
  std::pair<std::string, bool> result{"", true};

  if (output_buffer_.empty()) {
    return result;
  }

  auto eol_pos{output_buffer_.find(eol_str)};
  if (eol_pos == output_buffer_.npos) {
    switch (output_buffer_.front()) {
      case '.':
      case '!':
        result.first = output_buffer_.front();
        output_buffer_.erase(0);
        result.second = false;
    }
    return result;
  }

  result.first = output_buffer_.substr(0, eol_pos);
  result.second = false;
  output_buffer_.erase(0, eol_pos + 2);
  return result;
}

auto MockMega65::read(int bytes_to_read, int timeout_ms) -> std::string
{
  std::string result{output_buffer_.substr(0, bytes_to_read)};
  output_buffer_.erase(0, bytes_to_read);
  return result;
}

void MockMega65::flush_rx_buffers() { output_buffer_.clear(); }

auto MockMega65::process_load_bytes(std::span<const char> buffer) -> std::span<const char>
{
  if (load_remaining_bytes_ <= 0) {
    return buffer;
  }

  if (load_remaining_bytes_ > buffer.size()) {
    std::copy(buffer.begin(), buffer.end(), &memory_.at(load_addr_));
    load_remaining_bytes_ -= buffer.size();
    load_addr_ += buffer.size();
    return {};
  }

  std::copy_n(buffer.begin(), load_remaining_bytes_, &memory_.at(load_addr_));
  auto remaining_buffer{buffer.subspan(load_remaining_bytes_)};
  load_remaining_bytes_ = 0;
  output_buffer_.append(".");
  return remaining_buffer;
}

auto MockMega65::parse_help_cmd(std::string_view line) -> bool
{
  static const std::regex r(R"(^\s*\?\d*\s*$)");

  if (!std::regex_search(line.cbegin(), line.cend(), r)) {
    return false;
  }

  output_buffer_.append(line).append(eol_str);
  output_buffer_.append("MEGA65 Serial Monitor").append(eol_str).append(".");
  return true;
}

auto MockMega65::parse_memory_cmd(std::string_view line) -> bool
{
  static const std::regex r(R"(^\s*([mM])\s*([0-9a-fA-F]+)\s*$)");

  std::cmatch match;
  if (!regex_search(line, match, r)) {
    return false;
  }

  const auto& cmd_match{match[1]};
  const auto& address_match{match[2]};
  int address = str_to_int(address_match.str(), 16);
  int num_lines = *cmd_match.first == 'm' ? 1 : 32;
  throw_if<std::out_of_range>(
      address + num_lines * 16 >= memory_.size(),
      fmt::format("Memory request at address {} with size {} out of range", address, num_lines * 16));

  output_buffer_.append(line).append(eol_str);
  for (int i{0}; i < num_lines; ++i) {
    auto mem_range{std::span<std::uint8_t>(memory_).subspan(address, 16)};
    output_buffer_.append(fmt::format(":{:08X}:{:02X}{}", address, fmt::join(mem_range, ""), eol_str));
    address += 16;
  }
  output_buffer_.append(eol_str).append(".");
  return true;
}

auto MockMega65::parse_trace_cmd(std::string_view line) -> bool
{
  static const std::regex r(R"(^\s*t([01])\s*$)");

  std::cmatch match;
  if (!regex_search(line, match, r)) {
    return false;
  }

  const auto& param{match[1]};
  trace_mode_ = *param.first == '1';
  output_buffer_.append(line).append(eol_str).append(".");
  return true;
}

auto MockMega65::parse_reset_cmd(std::string_view line) -> bool
{
  static const std::regex r(R"(^\s*!\s*$)");

  if (!std::regex_search(line.cbegin(), line.cend(), r)) {
    return false;
  }

  output_buffer_.append(line).append(eol_str);
  output_buffer_.append("@MEGA65 Serial Monitor").append(eol_str).append(".");
  return true;
}

auto MockMega65::parse_load_cmd(std::string_view line) -> bool
{
  static const std::regex r(R"(^\s*[l]\s*([0-9a-fA-F]{1,4})\s+([0-9a-fA-F]{1,4})\s*$)");

  std::cmatch match;
  if (!regex_search(line, match, r)) {
    return false;
  }

  const auto& addr_begin_match{match[1]};
  const auto& addr_end_match{match[2]};
  load_addr_ = str_to_int(addr_begin_match.str(), 16);
  int addr_end = str_to_int(addr_end_match.str(), 16);
  load_remaining_bytes_ = addr_end - load_addr_;
  if (load_remaining_bytes_ < 0) {
    load_remaining_bytes_ += 0x10000;
  }
  output_buffer_.append(line).append(eol_str);
  if (load_remaining_bytes_ == 0) {
    output_buffer_.append(".");
  }
  return true;
}

}  // namespace m65dap::test::mock
