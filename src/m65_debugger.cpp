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
  if (reset_on_run)
  {
    reset_target();
    std::this_thread::sleep_for(2s);
  }
  else
  {
    // make sure serial debugger is not stopped
    execute_command("t0\n");
  }
}

M65Debugger::~M65Debugger()
{
  if (reset_on_disconnect_)
  {
    reset_target();
  }
}

void M65Debugger::set_target(const std::filesystem::path& prg_path)
{
  upload_prg_file(prg_path);

  auto dbg_file = prg_path;
  dbg_file.replace_extension("dbg");
  parse_debug_symbols(dbg_file);
}

void M65Debugger::run_target()
{
  simulate_keypresses("RUN\r");
}

void M65Debugger::pause()
{
  execute_command("t1\n");
  stopped_ = true;
  update_registers();
  if (event_handler_)
  {
    event_handler_->handle_debugger_stopped(StoppedReason::Pause);
  }
}

void M65Debugger::cont()
{
  execute_command("t0\n");
  stopped_ = false;
}

void M65Debugger::next()
{
  throw_if<std::runtime_error>(!stopped_, "Debugger not in stopped state");
  execute_command("\n");
  update_registers();
  if (event_handler_)
  {
    event_handler_->handle_debugger_stopped(StoppedReason::Step);
  }
}

auto M65Debugger::set_breakpoint(const std::filesystem::path& src_path, int line) -> bool
{
  if (!dbg_data_)
    return false;
  
  auto dbg_block_entry = dbg_data_->eval_breakpoint_line(src_path, line);
  if (!dbg_block_entry)
    return false;

  Breakpoint b {
    .src_path = src_path,
    .line = dbg_block_entry->line1,
    .pc = dbg_block_entry->start
  };
  execute_command(fmt::format("b{:X}\n", b.pc));
  breakpoint_ = std::make_unique<Breakpoint>(std::move(b));
  return true;
}

auto M65Debugger::get_breakpoint() const -> const Breakpoint*
{
  return breakpoint_.get();
}

auto M65Debugger::get_registers() -> Registers
{
  throw_if<std::runtime_error>(!stopped_, "Debugger not in stopped state");
  return current_registers_;
}

auto M65Debugger::get_pc() -> int
{
  return current_registers_.pc;
}

auto M65Debugger::get_current_source_position() -> SourcePosition
{
  SourcePosition result;

  if (dbg_data_)
  {
    auto entry = dbg_data_->get_block_entry(current_registers_.pc, 
                                            &result.segment,
                                            &result.block);
    if (entry)
    {
      result.src_path = dbg_data_->get_file(entry->file_index);
      result.line = entry->line1;
      return result;
    }
  }

  return result;  
}

void M65Debugger::do_event_processing()
{
  std::lock_guard sl(read_mutex_);
  auto result = conn_->read(1, 10);
  if (result.empty() || result.front() != '!') {
    return;
  }

  auto lines = get_lines_until_prompt();
  if (lines.size() < 3 || !lines[1].starts_with("PC ")) {
    execute_command("t0\n");      
    return;
  }
  std::istringstream sstr(lines[2]);
  sstr >> std::hex;
  int pc;
  sstr >> pc;
  if (pc != breakpoint_->pc) {
    execute_command("t0\n");
  }

  stopped_ = true;
  update_registers();
  if (event_handler_)
  {
    event_handler_->handle_debugger_stopped(StoppedReason::Breakpoint);
  }
}

void M65Debugger::sync_connection()
{
  std::scoped_lock sl(read_mutex_);

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
  std::scoped_lock sl(read_mutex_);

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
  if (lines.size() < 2 || !lines[1].starts_with("PC   A  X  Y  Z  B  SP"))
  {
    throw std::runtime_error("Unable to retrieve registers");
  }

  std::istringstream sstr(lines[2]);
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

  std::scoped_lock sl(read_mutex_);

  conn_->write(cmd);
  conn_->write(payload);

  get_lines_until_prompt();
}

void M65Debugger::parse_debug_symbols(const std::filesystem::path& dbg_path)
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
  std::scoped_lock sl(read_mutex_);

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
