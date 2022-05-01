#include <memory>

#include "connection_proxy.h"
#include "m65_debugger.h"
#include "mock_mega65.h"

struct EventHandler : public m65dap::M65Debugger::EventHandlerInterface {
  std::promise<void> stopped_event_promise;
  void handle_debugger_stopped(m65dap::M65Debugger::StoppedReason) { stopped_event_promise.set_value(); }
};

int main(int argc, char* argv[])
{
  if (argc != 2) {
    fmt::print(stderr, "Usage:\n  {} <serial-device>\n", std::filesystem::path(argv[0]).filename().string());
    return 1;
  }

  try {
    const std::string_view device{argv[1]};
    EventHandler stopped_handler;

    std::unique_ptr<m65dap::test::ConnectionProxy> conn{std::make_unique<m65dap::test::ConnectionProxy>(device)};
    // Flush rx buffer
    conn->read(65536, 100);
    auto debugger = std::make_unique<m65dap::M65Debugger>(
        std::move(conn), &stopped_handler, m65dap::StdoutLogger::instance(), conn->connected_with_xemu(), true, false);

    debugger->set_target("data/test.prg");
    debugger->set_breakpoint("data/test_main.asm", 79);
    debugger->run_target();
    stopped_handler.stopped_event_promise.get_future().wait();
    debugger->get_registers();
  }
  catch (const std::exception& e) {
    fmt::print(stderr, "Error: {}\n", e.what());
    return 1;
  }

  std::cout << "Test procedure completed successfully.\n";
  return 0;
}