#include "dap_logger.h"

#include <dap/protocol.h>

namespace m65dap {

DapLogger& DapLogger::instance()
{
  static DapLogger instance;
  return instance;
}

void DapLogger::open_file(const std::filesystem::path& log_file_path)
{
  log_file_ = dap::file(log_file_path.native().c_str());
}

void DapLogger::debug_out(std::string_view msg)
{
  auto inst = instance();
  if (inst.session_) {
    dap::OutputEvent event;
    event.output = msg;
    inst.session_->send(event);
  }
}

void DapLogger::write_log_file(std::string_view msg)
{
  auto inst = instance();
  if (inst.log_file_) {
    inst.log_file_->write(msg.data(), msg.length());
  }
}

}  // namespace m65dap
