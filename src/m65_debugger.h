#pragma once

#include "c64_debugger_data.h"
#include "connection.h"
#include "memory_cache.h"

namespace m65dap {

class M65Debugger {
 public:
  enum class StoppedReason { Pause, Step, Breakpoint };

  class EventHandlerInterface {
   public:
    virtual ~EventHandlerInterface() = default;

    virtual void handle_debugger_stopped([[maybe_unused]] StoppedReason reason){};
  };

  struct Registers {
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

  struct SourcePosition {
    std::filesystem::path src_path;
    int line{0};
    std::string segment;
    std::string block;
  };

  struct Breakpoint {
    std::filesystem::path src_path;
    int line{0};
    int pc{0};
  };

  struct EvaluateResult {
    std::string result_string;
    int address{-1};
  };

 private:
  friend class MemoryCache;

  using DebuggerTaskResult = std::optional<std::variant<EvaluateResult>>;
  using DebuggerTask = std::packaged_task<DebuggerTaskResult()>;

  EventHandlerInterface* event_handler_{nullptr};
  MemoryCache memory_cache_;
  std::unique_ptr<Connection> conn_;
  std::thread main_loop_thread_;
  std::promise<void> main_loop_exit_signal_;
  std::queue<DebuggerTask> debugger_tasks_;

  std::unique_ptr<C64DebuggerData> dbg_data_;
  bool is_xemu_{false};
  bool reset_on_disconnect_{true};
  bool stopped_{false};
  Registers current_registers_;
  std::optional<Breakpoint> breakpoint_;
  std::mutex task_queue_mutex_;

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
  void set_breakpoint(const std::filesystem::path& src_path, int line);
  void clear_breakpoint();
  auto get_breakpoint() const -> std::optional<Breakpoint> { return breakpoint_; }
  auto get_registers() -> Registers { return current_registers_; }
  auto get_pc() -> int { return current_registers_.pc; }
  auto get_current_source_position() -> SourcePosition;
  auto evaluate_expression(std::string_view expression, bool format_as_hex) -> EvaluateResult;

 private:
  void main_loop(std::future<void> future_exit_object);
  void do_event_processing();
  void check_breakpoint_by_pc();

  template <typename Func>
  DebuggerTaskResult run_task(Func f)
  {
    auto task = DebuggerTask(f);
    auto fut = task.get_future();
    {
      std::scoped_lock sl(task_queue_mutex_);
      debugger_tasks_.push(std::move(task));
    }
    return fut.get();
  }

  void sync_connection();
  void reset_target();
  void update_registers();
  auto update_registers(std::vector<std::string> lines) -> bool;

  void upload_prg_file(const std::filesystem::path& prg_path);
  void load_debug_symbols(const std::filesystem::path& dbg_path);
  void simulate_keypresses(std::string_view keys);
  auto get_lines_until_prompt() -> std::vector<std::string>;
  auto execute_command(std::string_view cmd) -> std::vector<std::string>;
  void process_async_event(std::vector<std::string>& lines);
  void get_memory_bytes(int address, std::span<std::byte> target);
  auto parse_address_line(std::string_view mem_string, std::span<std::byte> target) -> int;
};

}  // namespace m65dap
