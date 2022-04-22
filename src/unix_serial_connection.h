#pragma once

#include "unix_connection.h"

namespace m65dap {

class UnixSerialConnection : public UnixConnection {
 public:
  UnixSerialConnection(std::string_view port);
};

}  // namespace m65dap
