#include "mock_mega65.h"

namespace {

const std::string eol_str{"\r\n"};

}  // namespace
namespace m65dap::test::mock {

MockMega65::MockMega65(bool is_xemu) : memory_(384 * 1024), is_xemu_(is_xemu) {}

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
    next_cmd();
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
  if (parse_break_cmd(input_str)) {
    return;
  }
  if (parse_store_cmd(input_str)) {
    return;
  }
  if (parse_registers_cmd(input_str)) {
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

void MockMega65::append_prompt()
{
  if (is_xemu_) {
    output_buffer_.append(".").append(eol_str);
  }
  else {
    output_buffer_.append(eol_str).append(".");
  }
}

void MockMega65::next_cmd()
{
  ++current_reg_out_;
  output_buffer_.append(eol_str);
  if (is_xemu_) {
    append_prompt();
  }
  output_buffer_.append(eol_str);
  output_registers();
  append_prompt();
}

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
  append_prompt();
  return remaining_buffer;
}

auto MockMega65::parse_help_cmd(std::string_view line) -> bool
{
  static const std::regex r(R"(^\s*\?\d*\s*$)");

  if (!std::regex_search(line.cbegin(), line.cend(), r)) {
    return false;
  }

  output_buffer_.append(line).append(eol_str);
  if (is_xemu_) {
    output_buffer_.append("Xemu/MEGA65 Serial Monitor")
        .append(eol_str)
        .append("Warning: not 100% compatible with UART monitor of a *real* MEGA65 ...")
        .append(eol_str);
  }
  else {
    output_buffer_.append("MEGA65 Serial Monitor")
        .append(eol_str)
        .append("build GIT: development,20220305.00,ee4f29d")
        .append(eol_str);
  }
  append_prompt();
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
  int num_lines = *cmd_match.first == 'm' ? 1 : 16;
  throw_if<std::out_of_range>(
      address + num_lines * 16 >= memory_.size(),
      fmt::format("Memory request at address {} with size {} out of range", address, num_lines * 16));

  output_buffer_.append(line).append(eol_str);
  for (int i{0}; i < num_lines; ++i) {
    auto mem_range{std::span<std::uint8_t>(memory_).subspan(address, 16)};
    if (is_xemu_) {
      output_buffer_.append(fmt::format(":{:08X}:{:02X}\n", address, fmt::join(mem_range, "")));
    }
    else {
      output_buffer_.append(fmt::format(":{:08X}:{:02X}{}", address, fmt::join(mem_range, ""), eol_str));
    }
    address += 16;
  }
  append_prompt();
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
  output_buffer_.append(line).append(eol_str);
  append_prompt();
  return true;
}

auto MockMega65::parse_reset_cmd(std::string_view line) -> bool
{
  static const std::regex r(R"(^\s*!\s*$)");

  if (!std::regex_search(line.cbegin(), line.cend(), r)) {
    return false;
  }

  output_buffer_.append(line).append(eol_str);
  output_buffer_.append(
      "@\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\nMEGA65 Serial "
      "Monitor\r\nbuild GIT: development,20220305.00,ee4f29d\r\n");
  append_prompt();
  return true;
}

auto MockMega65::parse_load_cmd(std::string_view line) -> bool
{
  static const std::regex r(R"(^\s*l\s*([0-9a-fA-F]{1,4})\s+([0-9a-fA-F]{1,4})\s*$)");

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
    append_prompt();
  }
  return true;
}

auto MockMega65::parse_break_cmd(std::string_view line) -> bool
{
  static const std::regex r(R"(^\s*b\s*([0-9a-fA-F]{1,4})\s*$)");

  std::cmatch match;
  if (!regex_search(line, match, r)) {
    return false;
  }

  output_buffer_.append(line).append(eol_str);
  append_prompt();
  breakpoint_set_ = true;
  return true;
}

auto MockMega65::parse_store_cmd(std::string_view line) -> bool
{
  static const std::regex r(R"(^\s*s\s*([0-9a-fA-F]{1,4})((\s+[0-9a-fA-F]{1,2})+)\s*$)");

  std::cmatch match;
  if (!regex_search(line, match, r)) {
    return false;
  }

  output_buffer_.append(line).append(eol_str);
  append_prompt();

  if (line == "sD0 4") {
    // assuming RUN cmd
    running_ = true;
    if (breakpoint_set_) {
      if (!is_xemu_) {
        output_buffer_.append("!");
      }
      output_buffer_.append(eol_str);
      output_registers();
      if (!is_xemu_) {
        append_prompt();
      }
    }
  }

  return true;
}

auto MockMega65::parse_registers_cmd(std::string_view line) -> bool
{
  if (line != "r") {
    return false;
  }

  output_buffer_.append(line).append(eol_str);
  output_buffer_.append(eol_str);
  output_registers();
  append_prompt();

  return true;
}

void MockMega65::output_registers()
{
  if (is_xemu_) {
    output_buffer_.append("PC   A  X  Y  Z  B  SP   MAPH MAPL LAST-OP     P  P-FLAGS   RGP uS IO");
  }
  else {
    output_buffer_.append("PC   A  X  Y  Z  B  SP   MAPH MAPL LAST-OP In     P  P-FLAGS   RGP uS IO ws h RECA8LHC");
  }
  output_buffer_.append(eol_str);

  switch (current_reg_out_) {
    case 0:
      if (is_xemu_) {
        output_buffer_.append("2056 00 FF 00 00 00 01FF 0000 0000 9A       A1 00 N-E----C ")
            .append(eol_str)
            .append(",07772056");
      }
      else {
        output_buffer_.append("2058 12 FF 00 00 00 01FF 0000 0000 A912    00     21 ..E....C ...P 15 -  00 - .....l.c")
            .append(eol_str)
            .append(",07772058  85 02     STA   $02");
      }
      break;
    case 1:
      if (is_xemu_) {
        output_buffer_.append("205A 12 FF 00 00 00 01FF 0000 0000 85       21 00 --E----C ")
            .append(eol_str)
            .append(",0777205A");
      }
      else {
        output_buffer_.append("205A 12 FF 00 00 00 01FF 0000 0000 8502    00     21 ..E....C ...P 15 -  00 - .....l.c")
            .append(eol_str)
            .append(",0777205A  A9 34     LDA   #$34");
      }
      break;
    default:
      throw std::logic_error("Unable to provide register command output");
  }
  output_buffer_.append(eol_str);
}

}  // namespace m65dap::test::mock
