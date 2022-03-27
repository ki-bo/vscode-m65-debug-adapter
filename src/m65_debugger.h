#pragma once

#include "connection.h"

class M65Debugger
{
  std::unique_ptr<Connection> conn_;

public:
  M65Debugger(std::string_view serial_port_device);

  void set_target(const std::filesystem::path& prg_path);
};
