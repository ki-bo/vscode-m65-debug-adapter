#pragma once

#include "unix_connection.h"

class UnixDomainSocketConnection : public UnixConnection
{
public:
  UnixDomainSocketConnection(const std::filesystem::path& path);
};
