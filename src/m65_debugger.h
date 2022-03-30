#pragma once

#include "connection.h"

class M65Debugger
{
  std::unique_ptr<Connection> conn_;

public:
  M65Debugger(std::string_view serial_port_device, bool do_reset = false);

  void set_target(const std::filesystem::path& prg_path);
  void run_target();

private:
  void sync_connection();
  void reset_target();
  
  void simulate_keypresses(std::string_view keys);
  auto get_lines_until_prompt() -> std::vector<std::string>;
  std::vector<std::string> execute_command(std::string_view cmd);
  void process_event(const std::vector<std::string>& lines);

};
