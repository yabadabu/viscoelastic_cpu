#pragma once

// Std platform

#ifdef IN_PLATFORM_WINDOWS
#define _SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#define _USE_MATH_DEFINES
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define OS_NAME  "windows"
#include <windows.h>
#endif

// ----------------------------------------
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

#ifdef IN_PLATFORM_WINDOWS
#include <direct.h>         // _mkdir
#define unlink   _unlink
#define stat     _stat
#define mkdir    _mkdir
#endif

// ----------------------------------------
#define ENABLE_PROFILING 1

// ----------------------------------------
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui/imgui.h"

//// ----------------------------------------
#include "geometry/geometry.h"
#include "utils/utils.h"
#include "resources/resource.h"
#include "profile/profiling.h"
#include "memory/buffer.h"
#include "formats/json/json_file.h"
#include "time/timer.h"
#include "modules/module.h"

