#include "util.h"

namespace m65dap
{

std::vector<std::string> split(const std::string& str, char delim)
{
  std::vector<std::string> result;
  std::stringstream sstr(str);
  std::string item;

  while (std::getline(sstr, item, delim))
  {
    result.push_back(item);
  }

  return result;
}

} // namespace
