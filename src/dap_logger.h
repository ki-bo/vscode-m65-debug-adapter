#pragma once

#include <dap/io.h>
#include <dap/session.h>

namespace m65dap {

class DapLogger {
  std::shared_ptr<dap::Writer> log_file_;
  dap::Session* session_{nullptr};

  DapLogger() = default;

 public:
  static DapLogger& instance();

  static void write_log_file(std::string_view msg);

  static void debug_out(std::string_view msg);

  void open_file(const std::filesystem::path& log_file_path);

  void register_session(dap::Session* session) { session_ = session; }

  auto get_file() -> std::shared_ptr<dap::Writer> { return log_file_; }
};

}  // namespace m65dap
