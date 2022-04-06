#include "unix_domain_socket_connection.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>

UnixDomainSocketConnection::UnixDomainSocketConnection(const std::filesystem::path& path)
{
  std::string_view native_path = path.native();

  if (native_path.length() > 107)
  {
    throw std::runtime_error("Unix domain socket address exceeds maximum length of 107 characters");
  }

  fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd_ < 0)
  {
    throw std::runtime_error(fmt::format("Open error: {}", strerror (errno)));
  }

  if (::fcntl(fd_, F_SETFL, O_NONBLOCK) == -1)
  {
    throw std::runtime_error(fmt::format("Non-blocking mode error: {}", strerror (errno)));
  }

  sockaddr_un addr;
  std::memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, native_path.data());

  if (::connect(fd_, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0)
  {
    throw std::runtime_error(fmt::format("Connect error: {}", strerror (errno)));
  }
}
