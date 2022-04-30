#pragma once

#include <dap/protocol.h>
#include <dap/session.h>

#include "logger.h"
#include "m65_debugger.h"

namespace m65dap {

class M65DapSession : public M65Debugger::EventHandlerInterface, public LoggerInterface {
  std::unique_ptr<M65Debugger> debugger_;
  std::unique_ptr<dap::Session> session_;
  std::shared_ptr<dap::Writer> log_file_;
  std::promise<void> exit_promise_;
  bool client_supports_variable_type_{false};
  bool client_supports_memory_references_{false};

 public:
  M65DapSession(const std::filesystem::path& log_file = "");

  void run();

  // Event handlers of M65Debugger
  void handle_debugger_stopped(M65Debugger::StoppedReason reason) override;

  // Implements Logger::debug_out
  void debug_out(std::string_view msg) final;

 private:
  void register_error_handler();
  void register_request_handlers();
};

}  // namespace m65dap
