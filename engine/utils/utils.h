#pragma once

typedef unsigned char u8;
typedef unsigned long long u64;
typedef unsigned int uint32_t;
typedef uint32_t u32;                       // This is just what we need from <cstdint>
static constexpr u32 invalid_id = ~0;

// --------------------------------------------------
// Use the murmurhash3 hash
uint32_t ENGINE_API getID(const char* msg);
uint32_t ENGINE_API getID(const void* data, size_t nbytes);
ENGINE_API const char* getStr(uint32_t id);

// --------------------------------------------------
void ENGINE_API dbg(const char* fmt, ...);
int ENGINE_API fatal(const char* fmt, ...);

typedef void(*TSysOutputHandler)(const char* error);
TSysOutputHandler setDebugHandler(TSysOutputHandler new_handler);
TSysOutputHandler setFatalHandler(TSysOutputHandler new_handler);

template< typename T >
inline T mapRange( float v, float imin, float imax, T omin, T omax ) {
  return omin + ( omax - omin ) * ( ( v - imin )/ ( imax - imin ) );
}

template< typename T >
inline T mapUnitRange( float v, T omin, T omax ) {
  return omin + ( omax - omin ) * ( v );
}

#include "randoms.h"