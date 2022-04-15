#include "m65_dap_session.h"

#include "dap_logger.h"

using namespace std::chrono_literals;

namespace dap {

struct M65LaunchRequest : LaunchRequest {
  using Response = LaunchResponse;

  M65LaunchRequest() = default;
  ~M65LaunchRequest() = default;

  optional<string> program;
  optional<string> serialPort;
  optional<dap::boolean> resetBeforeRun;
  optional<dap::boolean> resetAfterDisconnect;
};

DAP_DECLARE_STRUCT_TYPEINFO(M65LaunchRequest);

DAP_IMPLEMENT_STRUCT_TYPEINFO(M65LaunchRequest,
                              "launch",
                              DAP_FIELD(restart, "__restart"),
                              DAP_FIELD(noDebug, "noDebug"),
                              DAP_FIELD(program, "program"),
                              DAP_FIELD(serialPort, "serialPort"),
                              DAP_FIELD(resetBeforeRun, "resetBeforeRun"),
                              DAP_FIELD(resetAfterDisconnect, "resetAfterDisconnect"));

}  // namespace dap

namespace {

const int thread_id = 1;

const int var_registers_id = 1;

}  // namespace

namespace m65dap {

M65DapSession::M65DapSession(const std::filesystem::path& log_file_path) : session_(dap::Session::create())
{
  if (!log_file_path.empty()) {
    DapLogger::instance().open_file(log_file_path);
  }

  DapLogger::instance().register_session(session_.get());

  register_error_handler();
  register_request_handlers();
}

void M65DapSession::run()
{
  std::shared_ptr<dap::Reader> in = dap::file(stdin, false);
  std::shared_ptr<dap::Writer> out = dap::file(stdout, false);

  auto log_file = DapLogger::instance().get_file();
  if (log_file) {
    session_->bind(spy(in, log_file), spy(out, log_file));
  }
  else {
    session_->bind(in, out);
  }

  exit_promise_.get_future().wait();
}

void M65DapSession::handle_debugger_stopped(M65Debugger::StoppedReason reason)
{
  dap::StoppedEvent event;
  switch (reason) {
    case M65Debugger::StoppedReason::Pause:
      event.reason = "pause";
      break;
    case M65Debugger::StoppedReason::Step:
      event.reason = "step";
      break;
    case M65Debugger::StoppedReason::Breakpoint:
      event.reason = "breakpoint";
      break;
    default:
      throw std::logic_error("Unimplemented stopped reason");
  }
  event.threadId = thread_id;
  session_->send(event);
}

void M65DapSession::register_error_handler()
{
  auto errorHandler = [&](const char* msg) {
    DapLogger::write_log_file(fmt::format("dap::Session error: {}\n", msg));
    exit_promise_.set_value();
  };

  session_->onError(errorHandler);
}

void M65DapSession::register_request_handlers()
{
  session_->registerHandler([&](const dap::InitializeRequest& req) {
    client_supports_variable_type_ = req.supportsVariableType.value(false);
    client_supports_memory_references_ = req.supportsMemoryReferences.value(false);

    dap::InitializeResponse res;
    res.supportsConfigurationDoneRequest = true;
    res.supportsValueFormattingOptions = true;
    res.supportsReadMemoryRequest = true;
    return res;
  });

  session_->registerSentHandler(
      [&](const dap::ResponseOrError<dap::InitializeResponse>&) { session_->send(dap::InitializedEvent()); });

  session_->registerHandler(
      [&](const dap::ConfigurationDoneRequest&) -> dap::ResponseOrError<dap::ConfigurationDoneResponse> {
        if (!debugger_) {
          return dap::Error("Debugger not initialized");
        }
        debugger_->run_target();
        return dap::ConfigurationDoneResponse();
      });

  session_->registerHandler([&](const dap::DisconnectRequest&) {
    debugger_.reset();
    return dap::DisconnectResponse();
  });

  session_->registerSentHandler([&](const dap::ResponseOrError<dap::DisconnectResponse>&) {
    session_->send(dap::InitializedEvent());
    exit_promise_.set_value();
  });

  session_->registerHandler([&](const dap::M65LaunchRequest& req) -> dap::ResponseOrError<dap::LaunchResponse> {
    if (!req.program.has_value()) {
      return dap::Error("Launch request needs definition of 'program' executable location");
    }
    if (!req.serialPort.has_value()) {
      return dap::Error("Launch request needs definition of 'serialPort'");
    }
    dap::boolean reset_before_run = req.resetBeforeRun.has_value() ? req.resetBeforeRun.value() : dap::boolean(false);
    dap::boolean reset_after_disconnect =
        req.resetAfterDisconnect.has_value() ? req.resetAfterDisconnect.value() : dap::boolean(true);

    try {
      debugger_ = std::make_unique<M65Debugger>(req.serialPort.value(), this, reset_before_run, reset_after_disconnect);
    }
    catch (const std::exception& e) {
      return dap::Error(fmt::format("Can't open connection to '{}'\n{}", req.serialPort.value(), e.what()));
    }

    if (!std::filesystem::exists(req.program.value())) {
      return dap::Error("Can't find program '%s'", req.program.value().c_str());
    }

    try {
      debugger_->set_target(req.program.value());
    }
    catch (const std::exception& e) {
      return dap::Error("Error launching target: '%s'", e.what());
    }

    dap::LaunchResponse res;
    return res;
  });

  session_->registerHandler([](const dap::ThreadsRequest&) {
    dap::ThreadsResponse response;
    dap::Thread thread;
    thread.id = thread_id;
    thread.name = "MEGA65Thread";
    response.threads.push_back(thread);
    return response;
  });

  session_->registerHandler(
      [&](const dap::SetBreakpointsRequest& req) -> dap::ResponseOrError<dap::SetBreakpointsResponse> {
        if (!debugger_) {
          return dap::Error("Debugger not initialized");
        }

        dap::SetBreakpointsResponse response;
        if (!req.breakpoints.has_value()) {
          return response;
        }

        const std::filesystem::path src_path = req.source.path.value("");
        const auto& breakpoints = req.breakpoints.value();
        for (const auto& b : breakpoints) {
          dap::Breakpoint result;

          result.line = b.line;
          dap::Source src;
          src.path = src_path;
          result.source = src;
          if (debugger_->get_breakpoint().has_value()) {
            result.verified = false;
          }
          else {
            result.verified = true;
            try {
              debugger_->set_breakpoint(src_path, result.line.value());
            }
            catch (...) {
              result.verified = false;
            }
          }
          response.breakpoints.push_back(result);
        }

        return response;
      });

  session_->registerHandler([&](const dap::PauseRequest&) {
    assert(debugger_);
    debugger_->pause();
    return dap::PauseResponse();
  });

  session_->registerHandler([&](const dap::ContinueRequest&) {
    assert(debugger_);
    debugger_->cont();
    return dap::ContinueResponse();
  });

  session_->registerHandler([&](const dap::NextRequest&) {
    assert(debugger_);
    debugger_->next();
    return dap::NextResponse();
  });

  session_->registerHandler([&](const dap::StackTraceRequest&) -> dap::ResponseOrError<dap::StackTraceResponse> {
    if (!debugger_) {
      return dap::Error("Debugger not initialized");
    }
    auto src_pos = debugger_->get_current_source_position();

    dap::StackTraceResponse response;
    response.totalFrames = 1;
    dap::StackFrame frame;
    frame.id = 1;
    frame.name = fmt::format("{}::{}", src_pos.segment, src_pos.block);
    dap::Source src;
    src.sourceReference = 0;

    std::filesystem::path src_path{src_pos.src_path};
    src.name = src_pos.src_path.filename();
    src.path = src_pos.src_path.native();
    frame.source = src;
    frame.line = src_pos.line;

    response.stackFrames.push_back(frame);
    return response;
  });

  session_->registerHandler([](const dap::SourceRequest& req) {
    if (req.sourceReference != 0) {
      throw std::invalid_argument("Source reference != 0 not supported");
    }
    dap::SourceResponse response;
    response.content = "";
    return response;
  });

  session_->registerHandler([](const dap::ScopesRequest& req) {
    if (req.frameId != 1) {
      throw std::invalid_argument("Invalid scope id requested");
    }

    dap::ScopesResponse response;
    dap::Scope registers_scope;
    registers_scope.name = "Registers";
    registers_scope.presentationHint = "registers";
    registers_scope.variablesReference = var_registers_id;
    registers_scope.expensive = false;
    response.scopes.push_back(registers_scope);

    dap::Scope local_scope;
    local_scope.name = "Local Vars";
    local_scope.presentationHint = "locals";
    local_scope.variablesReference = 2;
    local_scope.expensive = false;
    response.scopes.push_back(local_scope);

    return response;
  });

  session_->registerHandler([&](const dap::VariablesRequest& req) {
    dap::VariablesResponse response;

    std::string format_str_byte{"0x{:0>2X}"};
    std::string format_str_word{"0x{:0>4X}"};
    /*if (req.format.has_value() && req.format.value().hex.value(false))
    {
      format_str = "${:X}";
    }
    else
    {
      format_str = "{:d}";
    }*/

    if (req.variablesReference == var_registers_id) {
      auto reg = debugger_->get_registers();
      dap::Variable v;
      if (client_supports_variable_type_) {
        v.type = "Byte";
      }
      v.name = "A";
      v.value = fmt::format(format_str_byte, reg.a);
      response.variables.push_back(v);
      v.name = "X";
      v.value = fmt::format(format_str_byte, reg.x);
      response.variables.push_back(v);
      v.name = "Y";
      v.value = fmt::format(format_str_byte, reg.y);
      response.variables.push_back(v);
      v.name = "Z";
      v.value = fmt::format(format_str_byte, reg.z);
      response.variables.push_back(v);
      v.name = "BP";
      v.value = fmt::format(format_str_byte, reg.b);
      response.variables.push_back(v);

      if (client_supports_variable_type_) {
        v.type = "Word";
      }
      v.name = "PC";
      v.value = fmt::format(format_str_word, reg.pc);
      response.variables.push_back(v);
      v.name = "SP";
      v.value = fmt::format(format_str_word, reg.sp);
      response.variables.push_back(v);

      v.name = "FL";
      v.type = "Flags";
      v.value = fmt::format("0x{:0>2X} ({})", reg.flags, reg.flags_string);
      response.variables.push_back(v);
    }

    return response;
  });

  session_->registerHandler([&](const dap::EvaluateRequest& req) -> dap::ResponseOrError<dap::EvaluateResponse> {
    if (!debugger_) {
      return dap::Error("Debugger not initialized");
    }

    dap::EvaluateResponse response;

    bool format_as_hex = req.format.has_value() ? req.format.value().hex.value(true) : dap::boolean(true);

    auto eval_result = debugger_->evaluate_expression(req.expression, format_as_hex);
    response.variablesReference = 0;
    response.result = eval_result.result_string;
    if (client_supports_memory_references_ && eval_result.address > -1) {
      response.memoryReference = fmt::format("${:X}", eval_result.address);
    }
    return response;
  });
}

}  // namespace m65dap
