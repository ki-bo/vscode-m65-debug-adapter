#include "m65_dap_session.h"

using namespace std::chrono_literals;

namespace dap
{
 
struct M65LaunchRequest : LaunchRequest
{
  using Response = LaunchResponse;

  M65LaunchRequest() = default;
  ~M65LaunchRequest() = default;

  optional<string> program;
  optional<string> serialPort;
};

DAP_DECLARE_STRUCT_TYPEINFO(M65LaunchRequest);

DAP_IMPLEMENT_STRUCT_TYPEINFO(M65LaunchRequest,
  "launch",
  DAP_FIELD(restart, "__restart"),
  DAP_FIELD(noDebug, "noDebug"),
  DAP_FIELD(program, "program"),
  DAP_FIELD(serialPort, "serialPort"));

} // namespace dap



M65DapSession::M65DapSession(const std::filesystem::path& log_file_path)
  : session_(dap::Session::create())
{
  if (!log_file_path.empty())
  {
    log_file_ = dap::file(log_file_path.native().c_str());
  }

  register_error_handler();
  register_init_request_handler();
  register_launch_request_handler();
  register_threads_request_handler();
}

void M65DapSession::run()
{
  std::shared_ptr<dap::Reader> in = dap::file(stdin, false);
  std::shared_ptr<dap::Writer> out = dap::file(stdout, false);

  if (log_file_)
  {
    session_->bind(spy(in, log_file_), spy(out, log_file_));
  }
  else
  {
    session_->bind(in, out);
  }

  exit_promise_.get_future().wait();
}

void M65DapSession::register_error_handler()
{
  auto errorHandler =
    [&](const char* msg)
    {
      if (log_file_)
      {
        dap::writef(log_file_, "dap::Session error: %s\n", msg);
        log_file_->close();
      }
      exit_promise_.set_value();
    };

  session_->onError(errorHandler);
}

void M65DapSession::register_init_request_handler()
{
  session_->registerHandler(
    [](const dap::InitializeRequest& req)
    {
      dap::InitializeResponse res;
      res.supportsConfigurationDoneRequest = true;
      return res;
    }
  );

  session_->registerSentHandler(
    [&](const dap::ResponseOrError<dap::InitializeResponse>&)
    {
      session_->send(dap::InitializedEvent());
    }
  );

  session_->registerHandler(
    [&](const dap::ConfigurationDoneRequest&)
    {
      return dap::ConfigurationDoneResponse();
    }
  );

  session_->registerHandler(
    [&](const dap::DisconnectRequest&)
    {
      debugger_.reset();
      return dap::DisconnectResponse();
    }
  );

  session_->registerSentHandler(
    [&](const dap::ResponseOrError<dap::DisconnectResponse>&)
    {
      session_->send(dap::InitializedEvent());
      exit_promise_.set_value();
    }
  );
}

void M65DapSession::register_launch_request_handler()
{
  session_->registerHandler(
    [&](const dap::M65LaunchRequest& req) -> dap::ResponseOrError<dap::LaunchResponse>
    {
      if (!req.program.has_value())
      {
        return dap::Error("Launch request needs definition of 'program' executable location");
      }
      if (!req.serialPort.has_value())
      {
        return dap::Error("Launch request needs definition of 'serialPort'");
      }

      try
      {
        debugger_ = std::make_unique<M65Debugger>(req.serialPort.value());
      }
      catch (...)
      {
        return dap::Error("Can't open serial connection on device '%s'", req.serialPort.value().c_str());
      }

      if (!std::filesystem::exists(req.program.value()))
      {
        return dap::Error("Can't find program '%s'", req.program.value().c_str());
      }

      try
      {
        debugger_->set_target(req.program.value());
      }
      catch (const std::exception& e)
      {
        return dap::Error("Error launching target: '%s'", e.what());
      }

      dap::LaunchResponse res;
      return res;
    }
  );
}

void M65DapSession::register_threads_request_handler()
{
  session_->registerHandler(
    [](const dap::ThreadsRequest&) {
      dap::ThreadsResponse response;
      dap::Thread thread;
      thread.id = 1;
      thread.name = "MEGA65Thread";
      response.threads.push_back(thread);
      return response;
    }
  );
}

