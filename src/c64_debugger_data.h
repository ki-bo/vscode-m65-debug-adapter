#pragma once

#include <tinyxml2.h>

struct BlockEntry
{
  int start;
  int end;
  int file_index;
  int line1;
  int col1;
  int line2;
  int col2;
};

struct Block
{
  std::string name;
  std::vector<BlockEntry> entries;
  int min_addr {0};
  int max_addr {0};

  bool is_in_range(int addr) const { return addr >= min_addr && addr <= max_addr; }
  auto get_block_entry(int addr) const -> const BlockEntry*
  {
    if (!is_in_range(addr))
    {
      return nullptr;
    }
    for (const auto& e : entries)
    {
      if (addr >= e.start && addr <= e.end)
      {
        return &e;
      }
    }
    return nullptr;
  }
};

struct Segment
{
  std::string name;
  std::string dest;
  std::vector<Block> blocks;

  auto get_block_entry(int addr, std::string* block_name = nullptr) const -> const BlockEntry*
  {
    for (const auto& b : blocks)
    {
      auto entry = b.get_block_entry(addr);
      if (entry != nullptr)
      {
        if (block_name)
        {
          *block_name = b.name;
        }
        return entry;
      }
    }
    return nullptr;
  }

};

class C64DebuggerData
{

  std::map<int, std::string> files_;
  std::vector<Segment> segments_;

public:
  C64DebuggerData(const std::filesystem::path& dbg_file);
  auto get_block_entry(int addr, 
                       std::string* segment = nullptr, 
                       std::string* block = nullptr) const -> const BlockEntry*;
  auto get_file(int idx) const -> std::string;
  auto get_file_index(const std::filesystem::path& src_path) const -> int;

  /**
   * @brief Calculates next possible line number to set breakpoint starting at "line"
   * 
   * @param src_path Source file to search in
   * @param line Starting line number to search for usable breakpoint line
   * @return BlockEntry Matching BlockEntry with line and address info for the provided src/line info (nullptr if none available)
   */
  auto eval_breakpoint_line(const std::filesystem::path& src_path, int line) const -> const BlockEntry*;

private:
  void parse_file_list(tinyxml2::XMLElement* root);
  void parse_segments(tinyxml2::XMLElement* root);
  auto parse_segment(tinyxml2::XMLElement* segment_element) -> Segment;
  auto parse_block(tinyxml2::XMLElement* block_element) -> Block;
};
