#include "m65_debugger.h"
#include "dap_logger.h"
#include "serial_connection.h"
#include "unix_domain_socket_connection.h"

using namespace std::chrono_literals;

M65Debugger::M65Debugger(std::string_view serial_port_device, 
                         EventHandlerInterface* event_handler,
                         bool reset_on_run,
                         bool reset_on_disconnect) :
  event_handler_(event_handler),
  reset_on_disconnect_(reset_on_disconnect)
{
  if (serial_port_device.starts_with("unix#"))
  {
    std::string_view socket_path = serial_port_device.substr(5);
    conn_ = std::make_unique<UnixDomainSocketConnection>(socket_path);
    is_xemu_ = true;
  }
  else
  {
    conn_ = std::make_unique<SerialConnection>(serial_port_device);
  }

  sync_connection();
  if (reset_on_run && !is_xemu_)
  {
    reset_target();
    std::this_thread::sleep_for(2s);
  }
  else
  {
    // make sure serial debugger is not stopped
    execute_command("t0\n");
  }

  main_loop_thread_ = std::thread(&M65Debugger::main_loop, this, std::move(main_loop_exit_signal_.get_future()));
}

M65Debugger::~M65Debugger()
{
  if (reset_on_disconnect_ && !is_xemu_)
  {
    reset_target();
  }

  main_loop_exit_signal_.set_value();
  main_loop_thread_.join();
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
    if (event_handler_)
    {
      auto f = std::async(std::launch::async, &EventHandlerInterface::handle_debugger_stopped, event_handler_, StoppedReason::Pause);
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
    if (!update_registers(lines)) {
      update_registers();
    }
    if (event_handler_)
    {
      auto f = std::async(std::launch::async, &EventHandlerInterface::handle_debugger_stopped, event_handler_,StoppedReason::Step);
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
      throw std::runtime_error(fmt::format("Can't set breakpoint at {}:{}", src_path.native(), line));
    }

    Breakpoint b {
      .src_path = src_path,
      .line = dbg_block_entry->line1,
      .pc = dbg_block_entry->start
    };
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

auto M65Debugger::get_breakpoint() const -> std::optional<Breakpoint>
{
  return breakpoint_;
}

auto M65Debugger::get_registers() -> Registers
{
  return current_registers_;
}

auto M65Debugger::get_pc() -> int
{
  return current_registers_.pc;
}

auto M65Debugger::get_current_source_position() -> SourcePosition
{
  SourcePosition result;

  if (!dbg_data_) {
    return result;
  }

  auto entry = dbg_data_->get_block_entry(current_registers_.pc, 
                                          &result.segment,
                                          &result.block);
  if (entry)
  {
    result.src_path = dbg_data_->get_file(entry->file_index);
    result.line = entry->line1;
  }
  return result;  
}

auto M65Debugger::evaluate_expression(std::string_view expression, bool format_as_hex) -> EvaluateResult
{
  auto task_result = run_task([&]() {
    throw_if<std::runtime_error>(!stopped_, "Debugger not in stopped state");
    EvaluateResult result;
    if (!dbg_data_) {
      return result;
    }

    const auto* label_entry = dbg_data_->get_label_info(expression);
    if (!label_entry) {
      return result;
    }

    auto data = get_memory_bytes(label_entry->address, 2);

    result.address = label_entry->address;
    result.result_string = fmt::format("{:0>2X} {:0>2X}", data[0], data[1]);
    return result;
  });

  return std::get<EvaluateResult>(task_result.value());
}

void M65Debugger::main_loop(std::future<void> future_exit_object)
{
  int cycle_counter = 0;

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
      }
      cycle_counter = 0;
    }

    do_event_processing();
    if (cycle_counter++ == 100) {
      check_breakpoint_by_pc();
      cycle_counter = 0;
    }
  }
  while (future_exit_object.wait_for(10ms) == std::future_status::timeout);
}

void M65Debugger::do_event_processing()
{
  auto result = conn_->read(1, 10);
  if (result.empty() || result.front() != '!') {
    return;
  }

  DapLogger::debug_out("Breakpoint triggered\n");
  auto lines = get_lines_until_prompt();
  if (!update_registers(lines) || current_registers_.pc != breakpoint_->pc) {
    execute_command("t0\n");      
    return;
  }

  stopped_ = true;
  if (event_handler_) {
    auto f = std::async(std::launch::async, &EventHandlerInterface::handle_debugger_stopped, event_handler_, StoppedReason::Breakpoint);
    f.wait();
  }
}

void M65Debugger::check_breakpoint_by_pc()
{
  if (!breakpoint_.has_value()) {
    return;
  }
  update_registers();
  if (current_registers_.pc == breakpoint_->pc) {
    stopped_ = true;
    if (event_handler_) {
      auto f = std::async(std::launch::async, &EventHandlerInterface::handle_debugger_stopped, event_handler_, StoppedReason::Breakpoint);
      f.wait();
    }
  }
}

void M65Debugger::sync_connection()
{
  int retries = 10;
  bool first_try = true;

  while (retries-- > 0)
  {
    auto cmd = is_xemu_ ? std::string("?\n") : fmt::format("?{}\n", retries);
    conn_->write(cmd);
    auto reply = conn_->read_line(500);
    if (reply.second)
    {
      // timeout
      DapLogger::debug_out(fmt::format("sync_connection() timeout, retries={}\n", retries));
    }
    else if (reply.first == cmd.substr(0, cmd.length() - 1))
    {
      std::vector<std::string> lines;
      bool timeout = false;

      try
      {
        lines = get_lines_until_prompt();
      }
      catch(const timeout_error&)
      {
        timeout = true;  
      }
      
      const std::string ident = is_xemu_ ? "Xemu/MEGA65 Serial Monitor" : "MEGA65 Serial Monitor";
      if (!timeout && lines.front().starts_with(ident))
      {
        DapLogger::debug_out(" === Successfully synced with target debugger ===\n");
        return;
      }
    }

    if (first_try && reply.second)
    {
      first_try = false;
      // write dummy data to exit hanging 'l' command
      std::array<char, 65536> dummy_array;
      std::fill(dummy_array.begin(), dummy_array.end(), ' ');
      dummy_array.back() = '\n';
      conn_->write(dummy_array);
    }
    conn_->flush_rx_buffers();
  }
  
  throw std::runtime_error("Unable to sync with Mega65 debugger interface");
}

void M65Debugger::reset_target()
{
  std::string cmd = "!\n";
  conn_->write(cmd);

  auto reply = conn_->read(4);
  auto expected = is_xemu_ ? "!\r\n?" : "!\r\n@";
  if (reply.empty() || reply != expected)
  {
    throw std::runtime_error("Unable to reset target");
  }

  reply = conn_->read(1, 10000);
  if (reply.empty())
  {
    throw std::runtime_error("Unable to reset target");
  }
  conn_->flush_rx_buffers();

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
  for (; it != lines.end() && it->empty(); ++it);
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
  for (int i = 0; i < 7; ++i)
  {
    if (current_registers_.flags_string[i] != '.')
    {
      current_registers_.flags |= val;
    }
    val >>= 1;
  }

  return true;
}

void M65Debugger::upload_prg_file(const std::filesystem::path& prg_path)
{
  auto file_size = std::filesystem::file_size(prg_path);
  if (file_size > 65536)
  {
    throw std::runtime_error(fmt::format("File size >64KB not supported"));
  }
  if (file_size < 3)
  {
    throw std::runtime_error("PRG file is too small");
  }

  std::vector<char> prg_data;
  prg_data.reserve(file_size);

  std::ifstream prg_file(prg_path, std::ios::binary);
  prg_data.assign(std::istreambuf_iterator<char>(prg_file),
                  std::istreambuf_iterator<char>());

  int load_address = static_cast<int>(prg_data[0]) +
                     static_cast<int>(prg_data[1]) * 256;

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

  auto finalize_cmd = 
    [&]() 
    {
      cmd += '\n';
      execute_command(cmd);
      cmd = fmt::format("s{:X} {:X}\n", 0xd0, count); // set number of keys in keyboard buffer
      execute_command(cmd);
    };

  for (auto& key : keys)
  {
    if (count == 0)
    {
      cmd = fmt::format("s{:X}", 0x2b0); // write bytes to keyboard buffer address
    }
    cmd += fmt::format(" {:0>2X}", static_cast<int>(key));
    ++count;
    if (count == max_per_line)
    {
      finalize_cmd();
      count = 0;
    }
  }

  if (count > 0)
  {
    finalize_cmd();
  }
}

auto M65Debugger::get_lines_until_prompt() -> std::vector<std::string>
{
  std::vector<std::string> lines;

  while (true)
  {
    auto result = conn_->read_line();
    if (result.second)
    {
      // timeout
      throw timeout_error();
    }

    if (result.first == ".")
    {
      return lines;
    }

    lines.push_back(std::move(result.first));
  }
}

std::vector<std::string> M65Debugger::execute_command(std::string_view cmd)
{
  conn_->write(cmd);

  while (true)
  {
    auto lines = get_lines_until_prompt();
    if (lines.empty())
    {
      throw std::runtime_error("Expected echo of cmd, but received empty reply before prompt");
    }

    if (lines.front() == cmd.substr(0, cmd.length() - 1))
    {
      // first line matches our cmd, this is our response data
      lines.erase(lines.begin());
      return lines;
    }

    // Received a line,  but it did not match our cmd, treat it as an event
    process_async_event(lines);
  }
}

void M65Debugger::process_async_event(const std::vector<std::string>& lines)
{
  // todo
}

auto M65Debugger::get_memory_bytes(int address, int count) -> std::vector<std::byte>
{
  assert(address >= 0 && count >= 0);
  std::vector<std::byte> result;
  result.reserve(count);
  int pos = 0;
  static const int bytes_per_line = 16;

  while (pos < count) {
    std::vector<std::string> lines;
    if (count - pos <= bytes_per_line) {
      lines = execute_command(fmt::format("m{:X}\n", address));
    } else {
      lines = execute_command(fmt::format("M{:X}\n", address));
    }
    for (const auto& line : lines) {
      if (pos >= count) {
        break;
      }
      auto needed = std::min(count - pos, bytes_per_line);
      auto ret_addr = parse_address_line(line, result, needed);
      if (ret_addr != address) {
        throw std::runtime_error("Unexpected address range provided by read memory command");
      }
      address += bytes_per_line;
      pos += needed;
    }
  }

  return result;
}

auto M65Debugger::parse_address_line(std::string_view mem_string, 
                                     std::vector<std::byte>& target, 
                                     int num_bytes) -> int
{
  if (mem_string.empty() || mem_string.length() != 41 || !mem_string.starts_with(":")) {
    throw std::runtime_error("Unexpected memory read response");
  }
  auto ret_addr = str_to_int(mem_string.substr(1, 7), 16);
  
  for (int idx {0}; idx < num_bytes; ++idx) {
    target.push_back(static_cast<std::byte>(str_to_int(mem_string.substr(9 + 2 * idx, 2), 16)));
  }

  return ret_addr;
}
