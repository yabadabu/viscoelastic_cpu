#include "platform.h"


// --------------------------------------------------
static void outputToStdout( const char* buf ) {
  printf( "%s", buf );
}

static void defaultFatalHandler( const char* buf ) {
  outputToStdout( buf );
  exit( -1 );
}

TSysOutputHandler dbg_handler = &outputToStdout;
TSysOutputHandler fatal_handler = &defaultFatalHandler;

// --------------------------------------------------
void dbg(const char* fmt, ...) {
  if (!dbg_handler)
    return;

  char buf[48 * 1024];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
  if (n < 0)
    buf[sizeof(buf) - 1] = 0x00;
  va_end(ap);
  (*dbg_handler)(buf);
}

// --------------------------------------------------
int fatal(const char* fmt, ...) {
  if (!fatal_handler)
    return 0;

  char buf[48 * 1024];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
  va_end(ap);

  (*fatal_handler)(buf);
  return 0;
}

// --------------------------------------------------
TSysOutputHandler setDebugHandler(TSysOutputHandler new_handler) {
  TSysOutputHandler prev = dbg_handler;
  dbg_handler = new_handler;
  return prev;
}

TSysOutputHandler setFatalHandler(TSysOutputHandler new_handler) {
  TSysOutputHandler prev = fatal_handler;
  fatal_handler = new_handler;
  return prev;
}

