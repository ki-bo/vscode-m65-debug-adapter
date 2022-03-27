#include "serial_connection.h"

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

}

void SerialConnection::write(std::span<char> buffer)
{
  int written = 0;

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

auto SerialConnection::read_line() -> std::string
{
  /*std::size_t pos;
  char tmp[1024];

  while ((pos = buffer_.find_first_of('\n')) == std::string::npos)
  {
    auto read_bytes = ::read(fd_, tmp, 1024);

    if (read_bytes < 0)
    {
      fmt::print("Error: {}\n", strerror(errno));
    }
    
    if (read_bytes > 0)
    {
      buffer_.append(tmp, read_bytes);
    }
  }

  auto line = buffer_.substr(0, pos);
  buffer_.erase(0, pos + 1);
  return line;*/
  return {};
}

auto SerialConnection::read(int n) -> std::string
{
  return {};
}


