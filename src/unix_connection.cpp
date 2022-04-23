#ifdef _POSIX_VERSION

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

}  // namespace m65dap

#endif  // _POSIX_VERSION
