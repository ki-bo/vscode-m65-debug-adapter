#pragma once

#include "unix_connection.h"

class SerialConnection : public UnixConnection
{

public:
  SerialConnection(std::string_view port);

};
