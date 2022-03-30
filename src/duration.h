#pragma once

class Duration
{
  using time_point = std::chrono::steady_clock::time_point;

  time_point start_time_;

public:
  Duration() : start_time_(std::chrono::steady_clock::now()) {}

  auto elapsed_ms() { 
    auto cur_time = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(cur_time - start_time_).count();
  }
};
