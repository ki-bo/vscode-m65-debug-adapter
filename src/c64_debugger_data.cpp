#include "c64_debugger_data.h"

#include <filesystem>

namespace m65dap {

C64DebuggerData::C64DebuggerData(const std::filesystem::path& dbg_file)
{
  tinyxml2::XMLDocument doc;
  throw_if<std::runtime_error>(doc.LoadFile(dbg_file.string().c_str()) != tinyxml2::XML_SUCCESS,
                               fmt::format("Unable to load xml file '{}'", from_u8string(dbg_file.u8string())));

  auto* root = doc.RootElement();
  throw_if<std::runtime_error>(
      !root || std::string(root->Name()) != "C64debugger" || !root->Attribute("version", "1.0"),
      "Invalid C64debugger .dbg file format or version");

  parse_file_list(root);
  parse_segments(root);
  parse_labels(root);
}

auto C64DebuggerData::get_block_entry(int addr, std::string* segment, std::string* block) const -> const BlockEntry*
{
  for (const auto& s : segments_) {
    std::string block_name;
    const auto* entry = s.get_block_entry(addr, &block_name);
    if (entry != nullptr) {
      if (segment) {
        *segment = s.name;
      }
      return entry;
    }
  }

  return nullptr;
}

auto C64DebuggerData::get_file(int idx) const -> std::string { return files_.at(idx); }

auto C64DebuggerData::get_file_index(const std::filesystem::path& src_path) const -> int
{
  for (auto it = files_.begin(); it != files_.end(); ++it) {
    std::filesystem::path current_path{it->second};
    if (std::filesystem::exists(current_path) && std::filesystem::equivalent(src_path, current_path)) {
      return it->first;
    }
  }

  return -1;
}

auto C64DebuggerData::get_label_info(std::string_view label) const -> const LabelEntry*
{
  auto it = std::find_if(labels_.begin(), labels_.end(), [&](const LabelEntry& e) { return e.name == label; });
  if (it == labels_.end()) {
    return nullptr;
  }
  return &(*it);
}

auto C64DebuggerData::eval_breakpoint_line(const std::filesystem::path& src_path, int line) const -> const BlockEntry*
{
  int file_index = get_file_index(src_path);
  if (file_index < 0) return nullptr;

  for (const auto& seg : segments_) {
    for (const auto& block : seg.blocks) {
      for (const auto& entry : block.entries) {
        if (entry.file_index != file_index) continue;
        if (line >= entry.line1 && line <= entry.line2) return &entry;
      }
    }
  }

  return nullptr;
}

void C64DebuggerData::parse_file_list(tinyxml2::XMLElement* root)
{
  auto* sources = root->FirstChildElement("Sources");
  throw_if<std::runtime_error>(!sources || !sources->Attribute("values", "INDEX,FILE"),
                               "Unsupported dbg Sources format");

  std::istringstream sources_sstr(sources->GetText());
  std::string line;
  while (std::getline(sources_sstr, line)) {
    if (trim(line).empty()) {
      continue;
    }
    auto items = split(line, ',');
    throw_if<std::runtime_error>(items.size() != 2, "Wrong Sources line format");
    int file_index = std::stoi(items[0]);
    files_[file_index] = items[1];
  }
}

void C64DebuggerData::parse_segments(tinyxml2::XMLElement* root)
{
  const auto xml_name{"Segment"};
  for (auto* elem = root->FirstChildElement(xml_name); elem; elem = elem->NextSiblingElement(xml_name)) {
    segments_.push_back(parse_segment(elem));
  }
}

auto C64DebuggerData::parse_segment(tinyxml2::XMLElement* segment_element) -> Segment
{
  throw_if<std::runtime_error>(!segment_element->Attribute("name") ||
                                   !segment_element->Attribute("values", "START,END,FILE_IDX,LINE1,COL1,LINE2,COL2"),
                               "Unsupported dbg Segment format");

  Segment result;
  result.name = segment_element->Attribute("name");

  const auto xml_name{"Block"};
  for (auto* elem = segment_element->FirstChildElement(xml_name); elem; elem = elem->NextSiblingElement(xml_name)) {
    result.blocks.push_back(parse_block(elem));
  }

  return result;
}

auto C64DebuggerData::parse_block(tinyxml2::XMLElement* block_element) -> Block
{
  throw_if<std::runtime_error>(!block_element->Attribute("name"), "Unsupported dbg Block format");

  Block result;
  result.name = block_element->Attribute("name");
  result.min_addr = std::numeric_limits<int>::max();
  result.max_addr = -1;

  std::istringstream block_sstr(block_element->GetText());
  std::string line;
  while (std::getline(block_sstr, line)) {
    if (trim(line).empty()) {
      continue;
    }
    auto items = split(line, ',');
    throw_if<std::runtime_error>(items.size() != 7, "Wrong Block line format");

    auto start = parse_c64_hex(items[0]);
    auto end = parse_c64_hex(items[1]);

    result.entries.emplace_back(BlockEntry{.start = start,
                                           .end = end,
                                           .file_index = stoi(items[2]),
                                           .line1 = stoi(items[3]),
                                           .col1 = stoi(items[4]),
                                           .line2 = stoi(items[5]),
                                           .col2 = stoi(items[6])});

    result.min_addr = std::min(start, result.min_addr);
    result.max_addr = std::max(end, result.max_addr);
  }

  return result;
}

void C64DebuggerData::parse_labels(tinyxml2::XMLElement* root)
{
  auto* labels_element = root->FirstChildElement("Labels");
  throw_if<std::runtime_error>(
      !labels_element ||
          !labels_element->Attribute("values", "SEGMENT,ADDRESS,NAME,START,END,FILE_IDX,LINE1,COL1,LINE2,COL2"),
      "Unsupported dbg Sources format");

  std::istringstream label_sstr(labels_element->GetText());
  std::string line;
  while (std::getline(label_sstr, line)) {
    if (trim(line).empty()) {
      continue;
    }
    auto items = split(line, ',');
    throw_if<std::runtime_error>(items.size() != 8, "Wrong Labels line format");

    auto address = parse_c64_hex(items[1]);

    labels_.emplace_back(LabelEntry{.segment = items[0],
                                    .address = address,
                                    .name = items[2],
                                    .file_index = stoi(items[3]),
                                    .line1 = stoi(items[4]),
                                    .col1 = stoi(items[5]),
                                    .line2 = stoi(items[6]),
                                    .col2 = stoi(items[7])});
  }
}

}  // namespace m65dap
