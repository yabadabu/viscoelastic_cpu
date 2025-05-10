#ifndef INC_RENDER_GPU_TRACES_H_
#define INC_RENDER_GPU_TRACES_H_

// Enable profile even in release builds
#ifdef PLATFORM_WINDOWS
#ifdef _DEBUG
#define ENABLE_GPU_PROFILE_DX11
#endif
#endif

#if defined(ENABLE_GPU_PROFILE_DX11)

#include <d3d9.h>         // D3DPERF_*

struct CGpuTrace {
  static void push(const char* name) {
    wchar_t wname[64];
    mbstowcs(wname, name, 64);
    D3DPERF_BeginEvent(D3DCOLOR_XRGB(192, 192, 255), wname);
  }
  static void pop() {
    D3DPERF_EndEvent();
  }
  static void setMarker(const char* name) {
    D3DCOLOR clr = D3DCOLOR_XRGB(255, 128, 255);
    wchar_t marker_wname[64];
    mbstowcs(marker_wname, name, 64);
    D3DPERF_SetMarker(clr, marker_wname);
  }

};

#else

struct CGpuTrace {
  static void push(const char* name) {}
  static void pop() {}
  static void setMarker(const char* name) {}
};

#endif

// ------------------------------------
struct CGpuScope {
  CGpuScope(const char* name) {
    CGpuTrace::push(name);
  }
  ~CGpuScope() {
    CGpuTrace::pop();
  }
};

#endif

