#pragma once

#include <dap/protocol.h>
#include <dap/session.h>

#include "m65_debugger.h"

class M65DapSession : public M65Debugger::EventHandlerInterface
{
  std::unique_ptr<M65Debugger> debugger_;
  std::unique_ptr<dap::Session> session_;
  std::promise<void> exit_promise_;
  bool client_supports_variable_type_ {false};
  bool client_supports_memory_references_ {false};

public:
  M65DapSession(const std::filesystem::path& log_file = "");
  
  void run();

  // Event handlers of M65Debugger
  void handle_debugger_stopped(M65Debugger::StoppedReason reason) override;

private:
  void register_error_handler();
  void register_request_handlers();
};
