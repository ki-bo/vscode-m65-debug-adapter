#pragma once

std::vector<std::string> split(const std::string&, char delim);

inline template<typename ExceptionType>
void throw_if(bool condition, std::string_view msg)
{
  if (condition)
  {
    throw ExceptionType(std::string(msg).c_str());
  }
}

const std::string white_space_chars {" \t\n\r\f\v"};

inline 
auto rtrim(std::string& str, const std::string& trim_chars = white_space_chars) -> std::string&
{
    str.erase(str.find_last_not_of(trim_chars) + 1);
    return str;
}

inline 
auto ltrim(std::string& str, const std::string& trim_chars = white_space_chars) -> std::string&
{
    str.erase(0, str.find_first_not_of(trim_chars));
    return str;
}

inline 
auto trim(std::string& str, const std::string& trim_chars = white_space_chars) -> std::string&
{
    return ltrim(rtrim(str, trim_chars), trim_chars);
}

inline
auto str_to_int(std::string_view str, int base) -> int
{
  int int_result;
  auto result = std::from_chars(str.data(), str.data() + str.length(), int_result, 16);
  if (result.ec == std::errc()) {
    return int_result;
  }
  throw std::invalid_argument(fmt::format("Invalid number conversion for string '{}'", str));
}
