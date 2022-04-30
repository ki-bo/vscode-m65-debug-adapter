#pragma once

namespace m65dap {

class LoggerInterface {
 public:
  virtual ~LoggerInterface() = default;
  virtual void debug_out(std::string_view msg) = 0;
};

class NullLogger : public LoggerInterface {
 public:
  void debug_out(std::string_view) override;
  static auto instance() -> NullLogger*;
};

class StdoutLogger : public LoggerInterface {
 public:
  void debug_out(std::string_view msg) override;
  static auto instance() -> StdoutLogger*;
};

}  // namespace m65dap
