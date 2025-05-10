#pragma once

typedef unsigned char u8;
typedef unsigned long long u64;
typedef unsigned int uint32_t;
typedef uint32_t u32;                       // This is just what we need from <cstdint>
static constexpr u32 invalid_id = ~0;

// --------------------------------------------------
void ENGINE_API dbg(const char* fmt, ...);
int ENGINE_API fatal(const char* fmt, ...);

typedef void(*TSysOutputHandler)(const char* error);
TSysOutputHandler setDebugHandler(TSysOutputHandler new_handler);
TSysOutputHandler setFatalHandler(TSysOutputHandler new_handler);

#include "randoms.h"
#include "make_id.h"
