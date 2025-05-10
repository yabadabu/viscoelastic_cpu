//
//  engine.h
//  Tube01
//
//  Created by Juan Abadia on 2/1/17.
//  Copyright © 2017 Juan Abadia. All rights reserved.
//

#ifndef INC_PLATFORM_H_
#define INC_PLATFORM_H_

// Std platform
#ifdef PLATFORM_WINDOWS

#define _SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#define _USE_MATH_DEFINES
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define OS_NAME         "windows"
#include <windows.h>

#define ENGINE_API 

// ----------------------------------------
#define __ENABLE_PROFILING__

#else

#include <mach/mach.h>
#include <mach/mach_time.h>
#include <sys/time.h>
#include <sys/stat.h>     // stat
#include <unistd.h>       // unlink
#define __forceinline
#define __declspec(arg)
#define ENGINE_API

#ifdef PLATFORM_IOS
#define OS_NAME         "ios"
#else
#define OS_NAME         "osx"
#endif

#endif

// C++ std
#include <cstdint>
#include <cassert>
#include <cstdint>
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
////#include "formats/json/json_io.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui/imgui.h"
//
//// ----------------------------------------
//#define ENGINE_SUPPORT_BUFFER_DOC_PREFIX
//
#include "geometry/geometry.h"
//#include "input/input.h"
#include "utils/utils.h"
#include "resources/resource.h"
#include "profile/profiling.h"
#include "memory/buffer.h"
#include "time/timer.h"
//#include "resources/resources_manager.h"
#include "modules/module.h"
//
//#define REFLECTOR_API ENGINE_API
//#define REFLECTOR_NAMESPACE meta
//#include "meta/meta_reflection.h"
//
//#include "formats/json/json_geometry.h"
//#include "formats/text/text_resource.h"
//
//#if defined( _USRDLL ) || defined( COMPILING_PLUGIN )
//// Required for the reflection to work properly
//#include "engine_types.h"
//#endif

#endif 
