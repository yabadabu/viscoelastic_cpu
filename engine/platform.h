#pragma once

// Std platform

#define _SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#define _USE_MATH_DEFINES
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define OS_NAME  "windows"
#include <windows.h>

#define ENGINE_API 

// ----------------------------------------
#define __ENABLE_PROFILING__

// C++ std
#include <cstdint>
#include <cassert>
#include <cstdio>
#include <cstdarg>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <cfloat>
#include <mutex>
#include <algorithm>

#ifdef PLATFORM_WINDOWS
#include <direct.h>         // _mkdir
#endif

// ----------------------------------------
#ifdef PLATFORM_WINDOWS
#define __ENABLE_PROFILING__
#else
#undef __ENABLE_PROFILING__
#endif

#ifdef PLATFORM_WINDOWS
#define unlink   _unlink
#define stat     _stat
#define mkdir    _mkdir
#endif


// ----------------------------------------
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui/imgui.h"

//// ----------------------------------------
#include "geometry/geometry.h"
#include "utils/utils.h"
#include "resources/resource.h"
#include "profile/profiling.h"
#include "memory/buffer.h"
#include "time/timer.h"
#include "modules/module.h"

