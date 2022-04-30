#ifdef _POSIX_VERSION

#include "unix_connection.h"

#include <unistd.h>

#include "duration.h"

using namespace std::chrono_literals;

namespace m65dap {

void UnixConnection::write(std::span<const char> buffer)
{
  int written = 0;
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
