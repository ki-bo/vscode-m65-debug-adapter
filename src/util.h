#pragma once

namespace m65dap {

std::vector<std::string> split(const std::string&, char delim);

template <typename ExceptionType>
void throw_if(bool condition, std::string_view msg)
{
  if (condition) {
    throw ExceptionType(std::string(msg).c_str());
  }
}

inline auto parse_c64_hex(std::string_view str) -> int
{
  m65dap::throw_if<std::runtime_error>(str.empty() || str.front() != '$', "Unexpected hex format");
  return std::stoi(std::string(str.substr(1)), nullptr, 16);
}

const std::string white_space_chars{" \t\n\r\f\v"};

inline auto rtrim(std::string& str, const std::string& trim_chars = white_space_chars) -> std::string&
{
  str.erase(str.find_last_not_of(trim_chars) + 1);
  return str;
}

inline auto ltrim(std::string& str, const std::string& trim_chars = white_space_chars) -> std::string&
{
  str.erase(0, str.find_first_not_of(trim_chars));
  return str;
}

inline auto trim(std::string& str, const std::string& trim_chars = white_space_chars) -> std::string&
{
  return ltrim(rtrim(str, trim_chars), trim_chars);
}

inline auto str_to_int(std::string_view str, int base = 10) -> int
{
  int int_result;
  auto result = std::from_chars(str.data(), str.data() + str.length(), int_result, base);
  if (result.ec == std::errc()) {
    return int_result;
  }
  throw std::invalid_argument(fmt::format("Invalid number conversion for string '{}'", str));
}

inline auto replace_all(std::string& str, std::string_view find_str, std::string_view replace_str) -> std::string&
{
  std::string::size_type pos{};
  while ((pos = str.find(find_str.data(), pos, find_str.length())) != str.npos) {
    str.replace(pos, find_str.length(), replace_str.data(), replace_str.length());
    pos += replace_str.length();
  }
  return str;
}

inline auto to_word(std::byte* ptr) -> int { return std::to_integer<int>(ptr[0]) + 256 * std::to_integer<int>(ptr[1]); }

}  // namespace m65dap
