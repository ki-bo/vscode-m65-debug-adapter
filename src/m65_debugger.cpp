#include "m65_debugger.h"

#include "duration.h"
#include "serial_connection.h"
#include "unix_domain_socket_connection.h"
#include "unix_serial_connection.h"

using namespace std::chrono_literals;

namespace m65dap {

M65Debugger::M65Debugger(std::string_view serial_port_device,
                         EventHandlerInterface* event_handler,
                         LoggerInterface* logger,
                         bool reset_on_run,
                         bool reset_on_disconnect) :
    event_handler_(event_handler),
    logger_(logger), memory_cache_(this), reset_on_disconnect_(reset_on_disconnect)
{
  if (logger_ == nullptr) {
    logger_ = NullLogger::instance();
  }

#ifdef _POSIX_VERSION
  if (serial_port_device.starts_with("unix#")) {
    std::string_view socket_path = serial_port_device.substr(5);
    conn_ = std::make_unique<UnixDomainSocketConnection>(socket_path);
    is_xemu_ = true;
  }
  else {
    conn_ = std::make_unique<UnixSerialConnection>(serial_port_device);
    flush_rx_buffers();
  }
#endif

#ifdef _WIN32
  conn_ = std::make_unique<SerialConnection>(serial_port_device);
#endif

  initialize(reset_on_run);
}

M65Debugger::M65Debugger(std::unique_ptr<Connection> connection,
                         EventHandlerInterface* event_handler,
                         LoggerInterface* logger,
                         bool is_xemu,
                         bool reset_on_run,
                         bool reset_on_disconnect) :
    conn_(std::move(connection)),
    logger_(logger), event_handler_(event_handler), memory_cache_(this), is_xemu_(is_xemu),
    reset_on_disconnect_(reset_on_disconnect)
{
  if (logger_ == nullptr) {
    logger_ = NullLogger::instance();
  }

  initialize(reset_on_run);
}

M65Debugger::~M65Debugger()
{
  main_loop_exit_signal_.set_value();
  main_loop_thread_.join();
  if (reset_on_disconnect_ && !is_xemu_) {
    reset_target();
  }
}

void M65Debugger::set_target(const std::filesystem::path& prg_path)
{
  run_task([&]() -> DebuggerTaskResult {
    upload_prg_file(prg_path);

    auto dbg_file = prg_path;
    dbg_file.replace_extension("dbg");
    load_debug_symbols(dbg_file);
    return {};
  });
}

void M65Debugger::run_target()
{
  run_task([&]() -> DebuggerTaskResult {
    simulate_keypresses("RUN\r");
    return {};
  });
}

void M65Debugger::pause()
{
  run_task([&]() -> DebuggerTaskResult {
    execute_command("t1\n");
    stopped_ = true;
    update_registers();
    memory_cache_.invalidate();
    if (event_handler_) {
      auto f = std::async(std::launch::async, &EventHandlerInterface::handle_debugger_stopped, event_handler_,
                          StoppedReason::Pause);
      f.wait();
    }
    return {};
  });
}

void M65Debugger::cont()
{
  run_task([&]() -> DebuggerTaskResult {
    execute_command("t0\n");
    stopped_ = false;
    return {};
  });
}

void M65Debugger::next()
{
  run_task([&]() -> DebuggerTaskResult {
    throw_if<std::runtime_error>(!stopped_, "Debugger not in stopped state");
    auto lines = execute_command("\n");
    if (is_xemu_) {
      lines = get_lines_until_prompt();
    }
    if (!update_registers(lines)) {
      update_registers();
    }
    memory_cache_.refresh_accessed();
    if (event_handler_) {
      auto f = std::async(std::launch::async, &EventHandlerInterface::handle_debugger_stopped, event_handler_,
                          StoppedReason::Step);
      f.wait();
    }
    return {};
  });
}

void M65Debugger::set_breakpoint(const std::filesystem::path& src_path, int line)
{
  run_task([&]() -> DebuggerTaskResult {
    if (!dbg_data_) {
      throw std::runtime_error("Can't set breakpoint, no debug symbols loaded");
    }

    auto dbg_block_entry = dbg_data_->eval_breakpoint_line(src_path, line);
    if (!dbg_block_entry) {
      throw std::runtime_error(fmt::format("Can't set breakpoint at {}:{}", src_path.string(), line));
    }

    Breakpoint b{.src_path = src_path, .line = dbg_block_entry->line1, .pc = dbg_block_entry->start};
    execute_command(fmt::format("b{:X}\n", b.pc));
    breakpoint_ = std::make_optional<Breakpoint>(std::move(b));
    return {};
  });
}

void M65Debugger::clear_breakpoint()
{
  execute_command("b\n");
  breakpoint_.reset();
}

auto M65Debugger::get_current_source_position() const -> SourcePosition
{
  SourcePosition result;

  if (!dbg_data_) {
    return result;
  }

  auto entry = dbg_data_->get_block_entry(current_registers_.pc, &result.segment, &result.block);
  if (entry) {
    result.src_path = dbg_data_->get_file(entry->file_index);
    result.line = entry->line1;
  }
  return result;
}

void M65Debugger::write(std::span<const char> buffer)
{
  bool is_ascii =
      std::find_if(buffer.begin(), buffer.end(), [](const char c) { return static_cast<int>(c) <= 0; }) == buffer.end();

  if (is_ascii) {
    std::string debug_str(buffer.data(), buffer.size());
    replace_all(debug_str, "\n", "\\n");
    replace_all(debug_str, "\r", "\\r");
    logger_->debug_out(fmt::format("-> \"{}\"\n", debug_str));
  }
  else {
    logger_->debug_out(fmt::format("-> binary data (size {0:}/${0:X} bytes)\n", buffer.size_bytes()));
  }

  conn_->write(buffer);
}

auto M65Debugger::evaluate_expression(std::string_view expression, bool format_as_hex) -> EvaluateResult
{
  auto task_result = run_task([&]() {
    throw_if<std::runtime_error>(!stopped_, "Debugger not in stopped state");
    EvaluateResult result;
    if (!dbg_data_) {
      return result;
    }

    static const std::regex direct_regex(
        R"(^\s*(\$[0-9a-fA-F]{1,7}?|\w+)(?:\s*,\s*([xyz]))?(?:\s*,\s*([bwq]))?(?:\s*,\s*(\d+))?\s*$)",
        std::regex::icase | std::regex::optimize);
    static const std::regex indirect_regex(
        R"(^\s*\(\s*(\w+)\s*\)(?:\s*,\s*([xyz]))?(?:\s*,\s*([bwq]))?(?:\s*,\s*(\d+))?\s*$)",
        std::regex::icase | std::regex::optimize);

    std::cmatch match;

    bool indirect;
    if (regex_search(expression, match, direct_regex)) {
      indirect = false;
    }
    else if (regex_search(expression, match, indirect_regex)) {
      indirect = true;
    }
    else {
      return result;
    }

    const auto& label_match = match[1];
    const auto& index_match = match[2];
    const auto& type_match = match[3];
    const auto& num_elements_match = match[4];

    int type_size{1};
    if (type_match.matched) {
      char type_char = type_match.str().front();
      switch (type_char) {
        case 'b':
          type_size = 1;
          break;
        case 'w':
          type_size = 2;
          break;
        case 'q':
          type_size = 4;
          break;
        default:
          throw std::logic_error(fmt::format("Unexpected expression data type '{}'", match[2].str()));
      }
    }

    std::string_view label{label_match.str()};

    int num_elements{1};
    if (num_elements_match.matched) {
      num_elements = std::min(256, std::atoi(num_elements_match.str().c_str()));
    }

    static const std::regex address_regex(R"(^\$([0-9a-fA-F]{1,7})$)", std::regex::icase | std::regex::optimize);
    int address{0};
    if (regex_search(label, address_regex)) {
      address = parse_c64_hex(label);
    }
    else {
      const auto* label_entry = dbg_data_->get_label_info(label);
      if (!label_entry) {
        return result;
      }
      address = label_entry->address;
    }

    std::vector<std::byte> tmp(type_size * num_elements);
    memory_cache_.read(address, tmp);

    result.address = address;
    switch (type_size) {
      case 1:
        result.result_string.reserve(3 * num_elements);
        for (int idx{0}; idx < num_elements; ++idx) {
          result.result_string.append(fmt::format("{:02X} ", tmp[idx]));
        }
        break;
      case 2:
        result.result_string.reserve(5 * num_elements);
        for (int idx{0}; idx < num_elements * 2; idx += 2) {
          result.result_string.append(fmt::format("{:02X}{:02X} ", tmp[idx + 1], tmp[idx]));
        }
        break;
      case 4:
        result.result_string.reserve(9 * num_elements);
        for (int idx{0}; idx < num_elements * 4; idx += 4) {
          result.result_string.append(
              fmt::format("{:02X}{:02X}{:02X}{:02X} ", tmp[idx + 3], tmp[idx + 2], tmp[idx + 1], tmp[idx]));
        }
        break;
    }
    result.result_string.pop_back();
    return result;
  });

  return std::get<EvaluateResult>(task_result.value());
}

void M65Debugger::initialize(bool reset_on_run)
{
  sync_connection();
  if (reset_on_run && !is_xemu_) {
    reset_target();
    std::this_thread::sleep_for(3s);
  }
  else {
    // make sure serial debugger is not stopped
    execute_command("t0\n");
  }

  main_loop_thread_ = std::thread(&M65Debugger::main_loop, this, std::move(main_loop_exit_signal_.get_future()));
}

void M65Debugger::main_loop(std::future<void> future_exit_object)
{
  Duration duration_since_last_interaction;

  do {
    std::optional<DebuggerTask> next_task;
    {
      std::scoped_lock sl(task_queue_mutex_);
      if (!debugger_tasks_.empty()) {
        next_task = std::move(debugger_tasks_.front());
        debugger_tasks_.pop();
      }
      if (next_task.has_value()) {
        (*next_task)();
        duration_since_last_interaction.reset();
      }
    }

    do_event_processing();
    if (duration_since_last_interaction.elapsed_ms() > 1000) {
      check_breakpoint_by_pc();
      duration_since_last_interaction.reset();
    }
  } while (future_exit_object.wait_for(10ms) == std::future_status::timeout);
}

void M65Debugger::do_event_processing()
{
  auto result = read_line(0);
  if (result.second) {
    // No line available now
    return;
  }

  auto& line = result.first;
  if (line != "!" && !line.empty()) {
    throw std::runtime_error("Unexpected breakpoint trigger response");
  }

  std::vector<std::string> lines;
  if (is_xemu_) {
    result = read_line();
    throw_if<timeout_error>(result.second, "Timeout reading breakpoint registers header");
    lines.emplace_back(std::move(result.first));
    if (!lines.back().starts_with("PC   A")) {
      throw std::runtime_error("Expected register header in breakpoint response");
    }
    result = read_line();
    throw_if<timeout_error>(result.second, "Timeout reading breakpoint registers values");
    lines.emplace_back(std::move(result.first));
    result = read_line();
    throw_if<timeout_error>(result.second, "Timeout reading breakpoint memory location");
    lines.emplace_back(std::move(result.first));
    if (!lines.back().starts_with(",0777")) {
      throw std::runtime_error("Expected register header in breakpoint response");
    }
  }
  else {
    // Real HW monitor
    lines = get_lines_until_prompt();
  }

  handle_breakpoint(lines);
}

void M65Debugger::check_breakpoint_by_pc()
{
  if (!breakpoint_.has_value() || stopped_) {
    return;
  }
  update_registers();
  if (is_breakpoint_trigger_valid()) {
    stopped_ = true;
    if (event_handler_) {
      auto f = std::async(std::launch::async, &EventHandlerInterface::handle_debugger_stopped, event_handler_,
                          StoppedReason::Breakpoint);
      f.wait();
    }
  }
}

auto M65Debugger::read_line(int timeout_ms) -> std::pair<std::string, bool>
{
  std::size_t pos;
  char tmp[1024];

  Duration t;

  while ((pos = buffer_.find_first_of("\n")) == std::string::npos) {
    if (!buffer_.empty()) {
      if (buffer_.front() == '.') {
        // Prompt found, no eol will follow, treat it as line
        logger_->debug_out("Prompt (.) found\n");
        buffer_.erase(0);
        return {".", false};
      }
      if (buffer_.front() == '!') {
        // Breakpoint found, no eol will follow, treat it as line
        logger_->debug_out("Breakpoint trigger (!) found\n");
        buffer_.erase(0);
        return {"!", false};
      }
    }

    auto read_data = conn_->read(1024, timeout_ms);

    if (read_data.empty()) {
      return {{}, true};
    }
    else {
      buffer_.append(read_data);
    }
  }

  if (is_xemu_) {
    if (buffer_.starts_with(".\r\n")) {
      logger_->debug_out("Prompt (.) found (Xemu style)\n");
      buffer_.erase(0, 3);
      return {".", false};
    }
  }
  else {
    if (buffer_.front() == '.') {
      logger_->debug_out("Prompt (.) found\n");
      buffer_.erase(0);
      return {".", false};
    }
  }

  auto line = buffer_.substr(0, pos);
  buffer_.erase(0, pos + 1);
  if (!line.empty() && line.back() == '\r') {
    line.pop_back();
  }
  last_line_was_empty_ = line.empty();

  auto dbg_str = line;
  replace_all(dbg_str, "\n", "\\n");
  replace_all(dbg_str, "\r", "\\r");
  logger_->debug_out(fmt::format("<- \"{}\"\n", dbg_str));
  return {line, false};
}

void M65Debugger::flush_rx_buffers()
{
  // Do a dummy read of 64K to flush the buffer
  auto dummy_str = conn_->read(65536, 100);
  if (!dummy_str.empty()) {
    buffer_.clear();
    logger_->debug_out(fmt::format("Flushing rx buffer ({0:}/${0:X} bytes)\n", dummy_str.size()));
  }
}

void M65Debugger::sync_connection()
{
  int retries = 10;
  bool first_try = true;

  while (retries-- > 0) {
    auto cmd = is_xemu_ ? std::string("?\n") : fmt::format("?{}\n", retries);
    conn_->write(cmd);
    auto reply = read_line(500);
    if (reply.second) {
      // timeout
      logger_->debug_out(fmt::format("sync_connection() timeout, retries={}\n", retries));
    }
    else if (reply.first == cmd.substr(0, cmd.length() - 1)) {
      std::vector<std::string> lines;
      bool timeout = false;

      try {
        lines = get_lines_until_prompt();
      }
      catch (const timeout_error&) {
        timeout = true;
      }

      const std::string ident = is_xemu_ ? "Xemu/MEGA65 Serial Monitor" : "MEGA65 Serial Monitor";
      if (!timeout && lines.front().starts_with(ident)) {
        logger_->debug_out(" === Successfully synced with target debugger ===\n");
        return;
      }
    }

    if (first_try && reply.second) {
      first_try = false;
      // write dummy data to exit hanging 'l' command
      std::array<char, 65536> dummy_array;
      std::fill(dummy_array.begin(), dummy_array.end(), ' ');
      dummy_array.back() = '\n';
      conn_->write(dummy_array);
    }
    flush_rx_buffers();
  }

  throw std::runtime_error("Unable to sync with Mega65 debugger interface");
}

void M65Debugger::reset_target()
{
  std::string cmd = "!\n";
  conn_->write(cmd);

  auto reply = conn_->read(4);
  auto expected = is_xemu_ ? "!\r\n?" : "!\r\n@";
  if (reply.empty() || reply != expected) {
    throw std::runtime_error("Unable to reset target");
  }

  reply = conn_->read(1, 10000);
  if (reply.empty()) {
    throw std::runtime_error("Unable to reset target");
  }
  flush_rx_buffers();

  sync_connection();
}

void M65Debugger::update_registers()
{
  auto lines = execute_command("r\n");
  if (!update_registers(lines)) {
    throw std::runtime_error("Unable to retrieve registers");
  }
}

auto M65Debugger::update_registers(std::vector<std::string> lines) -> bool
{
  auto it = lines.begin();
  for (; it != lines.end() && it->empty(); ++it)
    ;
  if (it == lines.end()) {
    return false;
  }

  if (!it->starts_with("PC   A  X  Y  Z  B  SP")) {
    return false;
  }

  if (++it == lines.end()) {
    return false;
  }

  std::istringstream sstr(*it);
  sstr >> std::hex;

  sstr >> current_registers_.pc;
  sstr >> current_registers_.a;
  sstr >> current_registers_.x;
  sstr >> current_registers_.y;
  sstr >> current_registers_.z;
  sstr >> current_registers_.b;
  sstr >> current_registers_.sp;
  sstr >> current_registers_.maph;
  sstr >> current_registers_.mapl;
  sstr >> current_registers_.last_op;
  sstr >> current_registers_.in;
  sstr >> current_registers_.p;
  sstr >> current_registers_.flags_string;
  sstr >> current_registers_.rgp_string;
  sstr >> current_registers_.us;
  sstr >> current_registers_.io;
  sstr >> current_registers_.ws;
  sstr >> current_registers_.h;
  sstr >> current_registers_.reca8lhc;

  int val = 0b10000000;
  current_registers_.flags = 0;
  for (int i = 0; i < 7; ++i) {
    if (current_registers_.flags_string[i] != '.') {
      current_registers_.flags |= val;
    }
    val >>= 1;
  }

  return true;
}

void M65Debugger::upload_prg_file(const std::filesystem::path& prg_path)
{
  auto file_size = std::filesystem::file_size(prg_path);
  if (file_size > 65536) {
    throw std::runtime_error(fmt::format("File size >64KB not supported"));
  }
  if (file_size < 3) {
    throw std::runtime_error("PRG file is too small");
  }

  std::vector<char> prg_data;
  prg_data.reserve(file_size);

  std::ifstream prg_file(prg_path, std::ios::binary);
  prg_data.assign(std::istreambuf_iterator<char>(prg_file), std::istreambuf_iterator<char>());

  int load_address = static_cast<int>(prg_data[0]) + static_cast<int>(prg_data[1]) * 256;

  std::span<char> payload(prg_data.data() + 2, prg_data.size() - 2);

  auto end_address_plus_one = load_address + payload.size();

  auto cmd = fmt::format("l{:X} {:X}\n", load_address, end_address_plus_one);

  conn_->write(cmd);
  conn_->write(payload);

  get_lines_until_prompt();
}

void M65Debugger::load_debug_symbols(const std::filesystem::path& dbg_path)
{
  dbg_data_ = std::make_unique<C64DebuggerData>(dbg_path);
}

void M65Debugger::simulate_keypresses(std::string_view keys)
{
  static const int max_per_line = 9;
  int count = 0;
  std::string cmd;

  auto finalize_cmd = [&]() {
    cmd += '\n';
    execute_command(cmd);
    cmd = fmt::format("sD0 {:X}\n", count);  // set number of keys in keyboard buffer
    execute_command(cmd);
  };

  for (auto& key : keys) {
    if (count == 0) {
      cmd = "s2B0";  // write bytes to keyboard buffer address
    }
    cmd += fmt::format(" {:02X}", static_cast<int>(key));
    ++count;
    if (count == max_per_line) {
      finalize_cmd();
      count = 0;
    }
  }

  if (count > 0) {
    finalize_cmd();
  }
}

auto M65Debugger::get_lines_until_prompt() -> std::vector<std::string>
{
  std::vector<std::string> lines;

  while (true) {
    auto result = read_line();
    throw_if<timeout_error>(result.second, "Timeout reading line");

    if (result.first == ".") {
      return lines;
    }

    lines.push_back(std::move(result.first));
  }
}

std::vector<std::string> M65Debugger::execute_command(std::string_view cmd)
{
  conn_->write(cmd);

  while (true) {
    auto lines = get_lines_until_prompt();
    if (lines.empty()) {
      throw std::runtime_error("Expected echo of cmd, but received empty reply before prompt");
    }

    if (lines.front() == cmd.substr(0, cmd.length() - 1)) {
      // first line matches our cmd, this is our response data
      lines.erase(lines.begin());
      return lines;
    }

    // Received a line,  but it did not match our cmd, treat it as an event
    handle_breakpoint(lines);
  }
}

void M65Debugger::handle_breakpoint(std::vector<std::string>& lines)
{
  std::erase_if(lines, [](const std::string& s) { return s.empty(); });
  if (lines[0] == "!") {
    lines.erase(lines.begin());
  }
  if (!lines[0].starts_with("PC   A  X  Y  Z  B  SP")) {
    throw std::runtime_error("Unexpected breakpoint trigger response");
  }

  logger_->debug_out("Breakpoint triggered\n");
  if (!update_registers(lines) || !is_breakpoint_trigger_valid()) {
    execute_command("t0\n");
    return;
  }

  memory_cache_.invalidate();
  stopped_ = true;
  if (event_handler_) {
    auto f = std::async(std::launch::async, &EventHandlerInterface::handle_debugger_stopped, event_handler_,
                        StoppedReason::Breakpoint);
    f.wait();
  }
}

void M65Debugger::get_memory_bytes(int address, std::span<std::byte> target)
{
  assert(address >= 0);
  const int count = target.size();
  int pos = 0;
  static const int bytes_per_line = 16;

  while (pos < count) {
    std::vector<std::string> lines;
    if (count - pos <= bytes_per_line) {
      lines = execute_command(fmt::format("m{:X}\n", address));
    }
    else {
      lines = execute_command(fmt::format("M{:X}\n", address));
    }
    for (const auto& line : lines) {
      if (line.empty()) {
        continue;
      }
      if (pos >= count) {
        break;
      }
      auto needed = std::min(count - pos, bytes_per_line);
      auto ret_addr = parse_address_line(line, target.subspan(pos, needed));
      if (ret_addr != address) {
        throw std::runtime_error("Unexpected address range provided by read memory command");
      }
      address += bytes_per_line;
      pos += needed;
    }
  }
}

auto M65Debugger::parse_address_line(std::string_view mem_string, std::span<std::byte> target) -> int
{
  assert(target.size() <= 16);
  static const std::regex r(R"(^:([0-9A-F]{1,8}):([0-9A-F]{32})$)", std::regex::optimize);
  std::cmatch match;
  throw_if<std::runtime_error>(!regex_search(mem_string, match, r), "Unexpected memory read response");
  auto ret_addr = str_to_int(match[1].str(), 16);

  const char* mem_bytes_ptr = static_cast<const char*>(&(*match[2].first));
  for (auto& val : target) {
    auto sv = std::string_view(mem_bytes_ptr, 2);
    val = static_cast<std::byte>(str_to_int(sv, 16));
    mem_bytes_ptr += 2;
  }

  return ret_addr;
}

auto M65Debugger::is_breakpoint_trigger_valid() -> bool
{
  std::byte cmd_at_breakpoint[5];
  memory_cache_.read(breakpoint_->pc, cmd_at_breakpoint);

  // int addr_on_stack{memory_cache_.read_word(current_registers_.sp + 1)};

  const auto& opcode{get_opcode(cmd_at_breakpoint[0])};
  if (opcode.mnemonic == Mnemonic::JSR || opcode.mnemonic == Mnemonic::BSR) {
    auto target_address = calculate_address(to_word(cmd_at_breakpoint + 1), opcode.mode, breakpoint_->pc + 2);
    logger_->debug_out(
        fmt::format("JSR/BSR breakpoint expecting {}, got PC {}\n", target_address, current_registers_.pc));
    return current_registers_.pc == target_address;
  }

  return true;
}

auto M65Debugger::calculate_address(int addr, AddressingMode mode, int pc) -> int
{
  switch (mode) {
    case AddressingMode::Absolute:
      return addr;
    case AddressingMode::AbsoluteIndirect:
      return memory_cache_.read_word(addr);
    case AddressingMode::AbsoluteIndirectX:
      return memory_cache_.read_word(addr + current_registers_.x);
    case AddressingMode::RelativeWord:
      return pc + (addr > 0x7fff ? addr - 0x10000 : addr);
  }
  throw std::logic_error("Unimplemented AddressingMode");
}

}  // namespace m65dap
