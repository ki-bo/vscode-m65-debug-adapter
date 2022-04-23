#ifdef _POSIX_VERSION

#include "unix_serial_connection.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>

#ifdef __APPLE__
#include <IOKit/serial/ioss.h>
#endif

namespace m65dap {

UnixSerialConnection::UnixSerialConnection(std::string_view port)
{
  fd_ = open(std::string(port).c_str(), O_RDWR | O_NOCTTY | O_SYNC);
  if (fd_ < 0) {
    throw std::runtime_error(fmt::format("Open error: {}", strerror(errno)));
  }

  struct termios tty;
  memset(&tty, 0, sizeof tty);
  if (tcgetattr(fd_, &tty) != 0) {
    throw std::runtime_error(fmt::format("error {} from tcgetattr", errno));
  }

#ifdef __APPLE__
  if (tcgetattr(fd_, &tty)) throw std::runtime_error("Failed to get terminal parameters");
  tty.c_cc[VMIN] = 0;
  tty.c_cc[VTIME] = 0;

  if (tcsetattr(fd_, TCSANOW, &tty)) throw std::runtime_error(fmt::format("error {} from tcsetattr", strerror(errno)));

  speed_t speed_apple = 2000000;
  if (ioctl(fd_, IOSSIOSPEED, &speed_apple) == -1) {
    throw std::runtime_error("Failed to set output baud rate using IOSSIOSPEED");
  }
#endif
}

}  // namespace m65dap

#endif  // _POSIX_VERSION
