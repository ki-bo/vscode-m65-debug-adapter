#pragma once

namespace m65dap {

class timeout_error : public std::runtime_error {
 public:
  timeout_error(const char* msg) : std::runtime_error(msg) {}
};

}  // namespace m65dap
