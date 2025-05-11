#pragma once

typedef unsigned char u8;
typedef unsigned long long u64;
typedef unsigned int uint32_t;
typedef uint32_t u32;                       // This is just what we need from <cstdint>

// --------------------------------------------------
void dbg(const char* fmt, ...);
int fatal(const char* fmt, ...);

typedef void(*TSysOutputHandler)(const char* error);
TSysOutputHandler setDebugHandler(TSysOutputHandler new_handler);
TSysOutputHandler setFatalHandler(TSysOutputHandler new_handler);

#include "randoms.h"
