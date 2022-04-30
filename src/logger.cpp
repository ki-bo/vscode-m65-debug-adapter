#include "logger.h"

namespace m65dap {

void NullLogger::debug_out(std::string_view) {}

auto NullLogger::instance() -> NullLogger*
{
  static NullLogger instance;
  return &instance;
}

void StdoutLogger::debug_out(std::string_view msg) { std::cout << msg; }

auto StdoutLogger::instance() -> StdoutLogger*
{
  static StdoutLogger instance;
  return &instance;
}

}  // namespace m65dap
