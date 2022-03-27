#pragma once

#include <dap/protocol.h>
#include <dap/session.h>

#include "m65_debugger.h"

class M65DapSession
{
  std::unique_ptr<M65Debugger> debugger_;
  std::unique_ptr<dap::Session> session_;
  std::shared_ptr<dap::Writer> log_file_;
  std::promise<void> exit_promise_;

public:
  M65DapSession(const std::filesystem::path& log_file = "");
  
  void run();

private:
  void register_error_handler();
  void register_init_request_handler();
  void register_launch_request_handler();
  void register_threads_request_handler();
};
