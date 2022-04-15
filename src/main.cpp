#include <dap/io.h>

#ifdef _WIN32
#include <fcntl.h>  // _O_BINARY
#include <io.h>     // _setmode
#endif
#include <unistd.h>

// Needed to attach debugger (we send SIGSTOP to ourselves to wait for debugger to connect)
#include <csignal>

#include "m65_dap_session.h"

int main()
{
#ifdef _WIN32
  // Set binary mode for stdin and stdout, prevents eol conversion logic
  _setmode(_fileno(stdin), _O_BINARY);
  _setmode(_fileno(stdout), _O_BINARY);
#endif

  // Wait for debugger to attach
#ifndef NDEBUG
#ifdef _POSIX_VERSION
  raise(SIGSTOP);
#endif
  m65dap::M65DapSession session("/tmp/daplog.txt");
#else
  m65dap::M65DapSession session;
#endif

  session.run();

  return 0;
}
