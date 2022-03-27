#pragma once

#include "connection.h"

class SerialConnection : public Connection
{
  int fd_;
  std::string buffer_;

public:
  SerialConnection(std::string_view port);

  void write(std::span<char> buffer) override;

  auto read_line() -> std::string override;
  auto read(int n) -> std::string override;
};
