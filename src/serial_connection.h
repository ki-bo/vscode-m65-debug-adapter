#pragma once

#include "unix_connection.h"

namespace m65dap {

class SerialConnection : public UnixConnection {
 public:
  SerialConnection(std::string_view port);
};

}  // namespace m65dap
