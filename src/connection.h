#pragma once

class Connection
{
public:
  virtual ~Connection() = default;

  virtual void write(std::span<char> buffer) = 0;

  virtual auto read_line() -> std::string = 0;
  virtual auto read(int n) -> std::string = 0;
};
