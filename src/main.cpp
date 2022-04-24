#include <dap/io.h>

#ifdef _WIN32
#include <windows.h>

#include <fcntl.h>  // _O_BINARY
#include <io.h>     // _setmode
#endif

// Needed to attach debugger (we send SIGSTOP to ourselves to wait for debugger to connect)
#include <csignal>

#include "m65_dap_session.h"

int main(int argc, const char* argv[])
{
#ifdef _WIN32
  // Set binary mode for stdin and stdout, prevents eol conversion logic
  _setmode(_fileno(stdin), _O_BINARY);
  _setmode(_fileno(stdout), _O_BINARY);
#endif

  std::filesystem::path log_path;

  // Wait for debugger to attach
#ifndef _NDEBUG
  log_path = std::filesystem::path(std::filesystem::temp_directory_path() / "daplog.txt");
#ifdef _POSIX_VERSION
  raise(SIGSTOP);
#else
  while (!::IsDebuggerPresent()) ::Sleep(100);
#endif
#endif

  m65dap::M65DapSession session(log_path);

  session.run();

  return 0;
}
