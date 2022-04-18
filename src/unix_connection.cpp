#include "unix_connection.h"

#include <unistd.h>

#include "dap_logger.h"
#include "duration.h"

using namespace std::chrono_literals;

namespace m65dap {

void UnixConnection::write(std::span<const char> buffer)
{
  int written = 0;

  bool is_ascii =
      std::find_if(buffer.begin(), buffer.end(), [](const char c) { return static_cast<int>(c) <= 0; }) == buffer.end();

  if (is_ascii) {
    std::string debug_str(buffer.data(), buffer.size());
    replace_all(debug_str, "\n", "\\n");
    replace_all(debug_str, "\r", "\\r");
    DapLogger::debug_out(fmt::format("-> \"{}\"\n", debug_str));
  }
  else {
    DapLogger::debug_out(fmt::format("-> binary data (size {0:}/${0:X} bytes)\n", buffer.size_bytes()));
  }

  while (written < buffer.size()) {
    auto n = ::write(fd_, buffer.data() + written, buffer.size() - written);
    if (n < 0 && errno != EAGAIN) {
      throw std::runtime_error(fmt::format("Error writing to serial port: {}", strerror(errno)));
    }
    else if (n > 0) {
      written += n;
      assert(written <= buffer.size());
    }
    else {
      std::this_thread::sleep_for(1ms);
    }
  }
}

auto UnixConnection::read_line(int timeout_ms) -> std::pair<std::string, bool>
{
  std::size_t pos;
  char tmp[1024];

  Duration t;

  while ((pos = buffer_.find_first_of("\n")) == std::string::npos) {
    if (!buffer_.empty()) {
      if (buffer_.front() == '.') {
        // Prompt found, no eol will follow, treat it as line
        DapLogger::debug_out("Prompt (.) found\n");
        buffer_.erase(0);
        return {".", false};
      }
      if (buffer_.front() == '!') {
        // Breakpoint found, no eol will follow, treat it as line
        DapLogger::debug_out("Breakpoint trigger (!) found\n");
        buffer_.erase(0);
        return {"!", false};
      }
    }

    auto read_bytes = ::read(fd_, tmp, 1024);

    if (read_bytes == 0 || (read_bytes == -1 && errno == EAGAIN)) {
      if (t.elapsed_ms() > timeout_ms) {
        return {{}, true};
      }
      std::this_thread::sleep_for(1ms);
    }
    else if (read_bytes < 0) {
      DapLogger::debug_out(fmt::format("Error: {}\n", strerror(errno)));
    }
    else {
      buffer_.append(tmp, read_bytes);
    }
  }

  if (buffer_.front() == '.') {
    DapLogger::debug_out("Prompt (.) found\n");
    buffer_.erase(0);
    return {".", false};
  }

  auto line = buffer_.substr(0, pos);
  buffer_.erase(0, pos + 1);
  if (!line.empty() && line.back() == '\r') {
    line.pop_back();
  }
  last_line_was_empty_ = line.empty();

  auto dbg_str = line;
  replace_all(dbg_str, "\n", "\\n");
  replace_all(dbg_str, "\r", "\\r");
  DapLogger::debug_out(fmt::format("<- \"{}\"\n", dbg_str));
  return {line, false};
}

auto UnixConnection::read(int bytes_to_read, int timeout_ms) -> std::string
{
  Duration t;
  int sum = 0;
  int n = 0;
  std::string tmp;
  tmp.resize(bytes_to_read);

  do {
    n = ::read(fd_, tmp.data() + sum, bytes_to_read - sum);
    if (n > 0) {
      sum += n;
    }
    else if (n < 0 && errno != EAGAIN) {
      throw std::runtime_error(fmt::format("MEGA65 debugger interface read error: {}", strerror(errno)));
    }
  } while (sum < bytes_to_read && t.elapsed_ms() < timeout_ms);

  if (sum < bytes_to_read) {
    tmp.resize(sum);
  }

  return tmp;
}

void UnixConnection::flush_rx_buffers()
{
  // Do a dummy read of 64K to flush the buffer
  auto dummy_str = read(65536, 100);
  if (!dummy_str.empty()) {
    buffer_.clear();
    DapLogger::debug_out(fmt::format("Flushing rx buffer ({0:}/${0:X} bytes)\n", dummy_str.size()));
  }
}

}  // namespace m65dap
