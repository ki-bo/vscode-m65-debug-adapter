#pragma once

#include "connection.h"
#include "c64_debugger_data.h"

class M65Debugger
{
public:
  enum class StoppedReason
  {
    Pause,
    Step,
    Breakpoint
  };

  class EventHandlerInterface
  {
  public:
    virtual ~EventHandlerInterface() = default;

    virtual void handle_debugger_stopped([[maybe_unused]] StoppedReason reason) {};
  };

  struct Registers
  {
    int pc;
    int a;
    int x;
    int y;
    int z;
    int b;
    int sp;
    int maph;
    int mapl;
    int last_op;
    int in;
    int p;

    std::string flags_string;
    int flags;

    std::string rgp_string;
    int rgp;

    int us;
    char io;
    int ws;
    char h;
    
    std::string reca8lhc;
  };

  struct SourcePosition
  {
    std::filesystem::path src_path;
    int line {0};
    std::string segment;
    std::string block;
  };

  struct Breakpoint
  {
    std::filesystem::path src_path;
    int line {0};
    int pc {0};
  };

private:
  EventHandlerInterface* event_handler_ {nullptr};
  std::unique_ptr<Connection> conn_;
  std::unique_ptr<C64DebuggerData> dbg_data_;
  bool is_xemu_ {false};
  bool reset_on_disconnect_ {true};
  bool stopped_ {false};
  Registers current_registers_;
  std::unique_ptr<Breakpoint> breakpoint_;
  std::recursive_mutex read_mutex_;

public:
  M65Debugger(std::string_view serial_port_device,
              EventHandlerInterface* event_handler,
              bool reset_on_run = false,
              bool reset_on_disconnect = true);

  ~M65Debugger();
  
  void set_target(const std::filesystem::path& prg_path);
  void run_target();
  void pause();
  void cont();
  void next();
  bool set_breakpoint(const std::filesystem::path& src_path, int line);
  auto get_breakpoint() const -> const Breakpoint*;
  auto get_registers() -> Registers;
  auto get_pc() -> int;
  auto get_current_source_position() -> SourcePosition;
  void do_event_processing();

private:
  void sync_connection();
  void reset_target();
  void update_registers();
  
  void upload_prg_file(const std::filesystem::path& prg_path);
  void parse_debug_symbols(const std::filesystem::path& dbg_path);
  void simulate_keypresses(std::string_view keys);
  auto get_lines_until_prompt() -> std::vector<std::string>;
  auto execute_command(std::string_view cmd) -> std::vector<std::string>;
  void process_async_event(const std::vector<std::string>& lines);

};
