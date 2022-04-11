#pragma once

#include "unix_connection.h"

namespace m65dap
{

class UnixDomainSocketConnection : public UnixConnection
{
public:
  UnixDomainSocketConnection(const std::filesystem::path& path);
};

} // namespace
