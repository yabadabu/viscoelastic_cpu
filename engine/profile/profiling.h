#pragma once

#ifndef ENABLE_PROFILING
#if defined(IN_PLATFORM_OSX) || defined(IN_PLATFORM_WINDOWS)
#define ENABLE_PROFILING 0
#else
#define ENABLE_PROFILING 0
#endif
#endif

#if !ENABLE_PROFILING

#define PROFILE_SCOPED()
#define PROFILE_SCOPED_NAMED(x)
#define PROFILE_START_CAPTURING(nFrames)    while(false) { }
#define PROFILE_BEGIN_SECTION(txt)          0
#define PROFILE_END_SECTION(n)
#define PROFILE_BEGIN_FRAME()

#else

namespace Profiling {

	uint32_t enterDataDataContainer(const char* txt);
    void     exitDataContainer( uint32_t id );

  void start();
  void stop();
  bool isCapturing();
  void ENGINE_API triggerCapture(uint32_t nframes);
  void ENGINE_API frameBegins();
  void ENGINE_API setCurrentThreadName(const char* new_name);

  struct ENGINE_API TScoped {
    uint32_t n;
    TScoped(const char* txt) {
      n = enterDataDataContainer(txt);
    }
    ~TScoped() {
        exitDataContainer(n);
    }
  };

#define STR_CONCAT_(x,y) x##y
#define STR_CONCAT(x,y) STR_CONCAT_(x,y)

#define PROFILE_SCOPED()                  PROFILE_SCOPED_NAMED(__FUNCSIG__)
#define PROFILE_SCOPED_NAMED(txt)         Profiling::TScoped STR_CONCAT(internal_profile_, __LINE__)(txt);
#define PROFILE_START_CAPTURING(nFrames)  Profiling::triggerCapture(nFrames)
#define PROFILE_BEGIN_SECTION(txt)        Profiling::enterDataDataContainer(txt)
#define PROFILE_END_SECTION(n)            Profiling::exitDataContainer(n)
#define PROFILE_BEGIN_FRAME()             Profiling::frameBegins()

}

#endif

