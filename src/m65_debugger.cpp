#include "m65_debugger.h"
#include "dap_logger.h"
#include "serial_connection.h"

using namespace std::chrono_literals;

M65Debugger::M65Debugger(std::string_view serial_port_device, 
                         bool reset_on_run,
                         bool reset_on_disconnect) :
  conn_(std::make_unique<SerialConnection>(serial_port_device)),
  reset_on_disconnect_(reset_on_disconnect)
{
  sync_connection();
  if (reset_on_run)
  {
    reset_target();
    std::this_thread::sleep_for(2s);
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

void M65Debugger::run_target()
{
  simulate_keypresses("RUN\r");
}

void M65Debugger::sync_connection()
{
  int retries = 10;
  bool first_try = true;

  while (retries-- > 0)
  {
    auto cmd = fmt::format("?{}\n", retries);
    conn_->write(cmd);
    auto reply = conn_->read_line(500);
    if (reply.second)
    {
      // timeout
      DapLogger::debug_out(fmt::format("sync_connection() timeout, retries={}\n", retries));
    }
    else if (reply.first == cmd.substr(0, cmd.length() - 1))
    {
      auto lines = get_lines_until_prompt();
      if (!lines.empty() && lines.front().starts_with("MEGA65 Serial Monitor"))
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
  if (reply.empty() || reply != "!\r\n@")
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
      return {};
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
      throw std::runtime_error("Timeout waiting for debugger response");
    }

    if (lines.front() == cmd.substr(0, cmd.length() - 1))
    {
      // first line matches our cmd, this is our response data
      lines.erase(lines.begin());
      return lines;
    }

    // Received a line,  but it did not match our cmd, treat it as an event
    process_event(lines);
  }
}

void M65Debugger::process_event(const std::vector<std::string>& lines)
{
  // todo
}
