#pragma once

namespace m65dap {

class M65Debugger;

class MemoryCache {
  M65Debugger* debugger_;

  struct LineInfo {
    int table_idx{0};
    int address{0};
    bool valid{false};
    bool accessed{false};
  };

  std::vector<std::byte> data_;
  std::vector<LineInfo> lines_;
  std::map<int, LineInfo*> address_view_;

 public:
  MemoryCache(M65Debugger* parent, int num_cache_lines = 512);

  void invalidate();
  void read(int address, std::span<std::byte> target);

  void refresh_accessed();

 private:
  auto ensure_valid_cache_line(int line_address) -> LineInfo*;
};

}  // namespace m65dap
