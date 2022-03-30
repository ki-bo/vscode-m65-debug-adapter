#include "serial_connection.h"
#include "duration.h"
#include "dap_logger.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <IOKit/serial/ioss.h>

using namespace std::chrono_literals;

SerialConnection::SerialConnection(std::string_view port)
{
  fd_ = open (std::string(port).c_str(), O_RDWR | O_NOCTTY | O_SYNC);
  if (fd_ < 0)
  {
    throw std::runtime_error(fmt::format("Open error: {}", strerror (errno)));
  }

  struct termios tty;
  memset (&tty, 0, sizeof tty);
  if (tcgetattr (fd_, &tty) != 0)
  {
    throw std::runtime_error(fmt::format("error {} from tcgetattr", errno));
  }

#ifdef __APPLE__
  if (tcgetattr(fd_, &tty)) throw std::runtime_error("Failed to get terminal parameters");
  tty.c_cc[VMIN]  = 0;
  tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

  if (tcsetattr(fd_, TCSANOW, &tty)) throw std::runtime_error(fmt::format("error {} from tcsetattr", strerror (errno)));

  speed_t speed_apple = 2000000;
  if (ioctl(fd_, IOSSIOSPEED, &speed_apple) == -1) {
    throw std::runtime_error("Failed to set output baud rate using IOSSIOSPEED");
  }
#endif

  flush_rx_buffers();
}

void SerialConnection::write(std::span<const char> buffer)
{
  int written = 0;

  bool is_ascii = std::find_if(buffer.begin(), buffer.end(), [](const char c) { return static_cast<int>(c) <= 0; }) == buffer.end();

  if (is_ascii)
  {
    std::string_view debug_str(buffer.data(), buffer.size());
    DapLogger::debug_out(fmt::format("-> \"{}\"\n", debug_str));
  }
  else
  {
    DapLogger::debug_out(fmt::format("-> binary data (size {0:}/${0:X} bytes)\n", buffer.size_bytes()));
  }

  while (written < buffer.size())
  {
    auto n = ::write(fd_, buffer.data() + written, buffer.size() - written);
    if (n < 0)
    {
      throw std::runtime_error(fmt::format("Error writing to serial port: {}", strerror(errno)));
    }
    else if (n > 0)
    {
      written += n;
      assert(written <= buffer.size());
    }
    else
    {
      std::this_thread::sleep_for(1ms);
    }
  }
}

auto SerialConnection::read_line(int timeout_ms) -> std::pair<std::string, bool>
{
  std::size_t pos;
  char tmp[1024];
  std::fill(tmp, tmp + 1024, 0xcb);

  Duration t;

  while ((pos = buffer_.find_first_of("\r\n")) == std::string::npos)
  {
    auto read_bytes = ::read(fd_, tmp, 1024);

    if (read_bytes < 0)
    {
      DapLogger::debug_out(fmt::format("Error: {}\n", strerror(errno)));
    }
    else if (read_bytes > 0)
    {
      buffer_.append(tmp, read_bytes);
    }
    else
    {
      if (t.elapsed_ms() > timeout_ms)
      {
        DapLogger::debug_out("Timeout in read_line()\n");
        return { {}, true };
      }
      std::this_thread::sleep_for(1ms);
    }
  }

  auto line = buffer_.substr(0, pos);
  buffer_.erase(0, pos + 2);
  if (line.empty())
  {
    if (!buffer_.empty() && buffer_[0] == '.')
    {
      // Prompt found (empty line following by '.')
      buffer_.erase(0, 1);
      DapLogger::debug_out("Prompt (.) found\n");
      return { ".", false };
    }
  }

  DapLogger::debug_out(fmt::format("<- \"{}\"\n", line));
  return { line, false };
}

auto SerialConnection::read(int bytes_to_read, int timeout_ms) -> std::string
{
  Duration t;
  int sum = 0;
  int n = 0;
  std::string tmp;
  tmp.resize(bytes_to_read);

  do
  {
    n = ::read(fd_, tmp.data() + sum, bytes_to_read - sum);
    sum += n;
  } while (sum < bytes_to_read && t.elapsed_ms() < 2000);

  if (sum < bytes_to_read)
  {
    tmp.resize(sum);
  }

  return tmp;
}

void SerialConnection::flush_rx_buffers()
{
  // Do a dummy read of 64K to flush the buffer
  auto dummy_str = read(65536, 100);
  if (!dummy_str.empty())
  {
    buffer_.clear();
    DapLogger::debug_out(fmt::format("Flushing rx buffer ({0:}/${0:X} bytes)\n", dummy_str.size()));
  }
}

