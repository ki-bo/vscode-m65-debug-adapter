#include "memory_cache.h"

#include "m65_debugger.h"

namespace {

const int bytes_per_cache_line = 256;

}

namespace m65dap {

MemoryCache::MemoryCache(M65Debugger* parent, int num_cache_lines) :
    debugger_(parent), data_(num_cache_lines * bytes_per_cache_line), lines_(num_cache_lines)
{
  for (int idx{0}; idx < num_cache_lines; ++idx) {
    lines_[idx].table_idx = idx;
  }
}

void MemoryCache::refresh_accessed()
{
  for (auto& entry : address_view_) {
    if (entry.second->accessed) {
      ensure_valid_cache_line(entry.first);
    }
    else {
      entry.second->address = 0;
      entry.second->valid = false;
    }
    entry.second->accessed = false;
  }
  std::erase_if(address_view_,
                [](const decltype(address_view_)::value_type& entry) { return !entry.second->accessed; });
}

void MemoryCache::invalidate()
{
  for (auto& line : lines_) {
    line.accessed = false;
    line.valid = false;
    line.address = 0;
  }
  address_view_.clear();
}

void MemoryCache::read(int address, std::span<std::byte> target)
{
  int line_address = address & ~(bytes_per_cache_line - 1);
  int line_offset = address % bytes_per_cache_line;
  int num_bytes = bytes_per_cache_line - line_offset;
  num_bytes = std::min(num_bytes, static_cast<int>(target.size()));
  auto target_it = target.begin();

  while (target_it != target.end()) {
    auto* line_info = ensure_valid_cache_line(line_address);
    line_info->accessed = true;
    auto it = data_.begin() + line_info->table_idx * bytes_per_cache_line + line_offset;
    std::copy(it, it + num_bytes, target_it);
    line_offset = 0;
    target_it += num_bytes;
    num_bytes = std::min(static_cast<int>(std::distance(target_it, target.end())), bytes_per_cache_line);
  }
}

auto MemoryCache::ensure_valid_cache_line(int line_address) -> LineInfo*
{
  assert(line_address % bytes_per_cache_line == 0);
  const auto it = address_view_.find(line_address);
  if (it != address_view_.end()) {
    return it->second;
  }

  auto table_it = std::find_if(lines_.begin(), lines_.end(), [](const LineInfo& i) { return i.valid == false; });
  if (table_it == lines_.end()) {
    table_it = std::find_if(lines_.begin(), lines_.end(), [](const LineInfo& i) { return i.accessed == false; });
    if (table_it == lines_.end()) {
      table_it = lines_.begin();
    }
  }

  auto cache_line_data = std::span(data_).subspan(table_it->table_idx * bytes_per_cache_line, bytes_per_cache_line);
  debugger_->get_memory_bytes(line_address, cache_line_data);
  LineInfo* info = &(*table_it);
  info->address = line_address;
  info->valid = true;
  address_view_[line_address] = info;

  return info;
}

}  // namespace m65dap
