#PLATFORM ?= IOS
PLATFORM ?= OSX

ROOT_APP_NAME:=demo
APP_NAME=${ROOT_APP_NAME}_${PLATFORM}

TARGET : ${APP_NAME}

CFLAGS=-Wall -c ${INCLUDE_OPTIONS} -DIN_PLATFORM_${PLATFORM}=1  
CFLAGS+= -D_LIBCPP_DISABLE_DEPRECATION_WARNINGS
CFLAGS+=

CONFIG_PATH=debug
ifdef RELEASE
CFLAGS+=-O2 -DNDEBUG
CONFIG_PATH=release
$(info Optimizations enabled)
else
CFLAGS+=-g -O0
endif

ifdef ASAN
$(info Asan enabled)
ASAN_FLAGS=-fsanitize=address -fno-omit-frame-pointer
endif

CFLAGS+=${ASAN_FLAGS}

CXXFLAGS=${CFLAGS} -std=c++20 -fno-exceptions -fno-objc-arc
cooker: CXXFLAGS := ${CFLAGS} -std=c++20 -fno-objc-arc

FRAMEWORKS=Foundation Metal MetalKit AudioToolbox GameKit

ifeq (${PLATFORM}, OSX)
	FRAMEWORKS+=QuartzCore Cocoa AppKit
	SRCS=main_osx imgui_impl_metal imgui_impl_osx apple_platform
	INCLUDE_PATHS_ARCH:=engine/render/metal engine/osx/metal-cpp
	ARCH_FLAGS=-target x86_64-apple-macos14s -mavx2 -DIMGUI_IMPL_METAL_CPP_EXTENSIONS -DIN_PLATFORM_APPLE=1
	LIBS+=-F/System/Library/Frameworks
	LNKFLAGS+=${ARCH_FLAGS}
else ifeq (${PLATFORM}, ARM)
	FRAMEWORKS+=QuartzCore Cocoa AppKit
	SRCS=main_osx apple_platform
	INCLUDE_PATHS_ARCH:=engine/render/metal engine/osx/metal-cpp
	ARCH_FLAGS=-target arm64-apple-macos14 -DIMGUI_IMPL_METAL_CPP_EXTENSIONS -DIN_PLATFORM_APPLE=1
	LIBS+=-F/System/Library/Frameworks
	LNKFLAGS+=${ARCH_FLAGS}
else ifeq (${PLATFORM}, LINUX)
	SRCS=main_linux
	INCLUDE_PATHS_ARCH:=
	ARCH_FLAGS=-mavx2
	LIBS+=-lm
else
	FRAMEWORKS+=UIKit CoreMotion Security CoreLocation
	SRCS=main_ios apple_platform
	INCLUDE_PATHS_ARCH:=engine/render/metal engine/osx/metal-cpp
	SYSROOT=$(shell xcrun --show-sdk-path --sdk iphoneos)
	ARCH_FLAGS=-target arm64-apple-ios14 -isysroot ${SYSROOT} -DIMGUI_IMPL_METAL_CPP_EXTENSIONS -DIN_PLATFORM_APPLE=1
	CXXFLAGS+=-arch arm64
	LIBS+=-lsoloud_static -lcurl -lz 
	LNKFLAGS+=${ARCH_FLAGS}
endif

INCLUDE_PATHS=. engine ${INCLUDE_PATHS_ARCH}
INCLUDE_OPTIONS+=$(foreach f,${INCLUDE_PATHS},-I$f)

CFLAGS+=${ARCH_FLAGS}
CXXFLAGS+=${ARCH_FLAGS}

ifeq (${PLATFORM}, OSX)
LIBS+=$(foreach f,${FRAMEWORKS},-framework $f)
endif

LIBS+=-lstdc++ ${ASAN_FLAGS}

OBJS_PATH=objs/${PLATFORM}/${CONFIG_PATH}

# Get All module sources
MODULE_SRCS=$(foreach f,$(shell find engine/modules -name "*.cpp"),${notdir ${basename $f}})

SRCS+=geometry transform camera angular sdf \
     render primitives \
     json json_file \
     utils profiling \
     resources_manager \
     imgui imgui_draw imgui_widgets imgui_tables imgui_demo ImGuizmo \
     viscoelastic viscoelastic_sim \
     ${MODULE_SRCS} \
     render_platform \

OBJS=$(foreach f,${SRCS},$(OBJS_PATH)/$(basename $f).o)

VPATH=${shell find engine -type d| grep -v objs | grep -v common | grep -v x64 | grep -v render/ } osx experiments tools

ifeq (${PLATFORM}, LINUX)
VPATH+=engine/render/null
endif

#$(info VPATH is ${VPATH})
#$(info OBJS is ${OBJS})

tools : cooker

#COMMON_DEPS=${wildcard *.h} Makefile
COMMON_DEPS=

$(OBJS_PATH)/%.o : %.c ${COMMON_DEPS} | $(OBJS_PATH)
	@echo C $@
	@$(CC) ${CFLAGS} -fobjc-arc -x objective-c $< -o $@

$(OBJS_PATH)/%.o : %.cpp ${COMMON_DEPS} | $(OBJS_PATH)
	@echo C++ $@
	@$(CC) ${CXXFLAGS} $< -o $@

$(OBJS_PATH)/%.o : %.mm ${COMMON_DEPS} | $(OBJS_PATH)
	@echo Compiling $@
	@$(CC) $(CXXFLAGS) $< -o $@

$(APP_NAME) : ${OBJS} | Makefile
	@echo Linking $@
	@$(CC) $+ ${LNKFLAGS} $(LIBS) -o $@

$(OBJS_PATH) :
	@echo Creating temporal folder $(OBJS_PATH)
	@mkdir -p $(OBJS_PATH)

assets : 
	@make --no-print-directory -C assets -j -r

SHADER_NAMES=basic sprites
SHADER_LIB=data/shaders.metallib
SHADERS_IRS_PATH=objs/OSX/shaders
SHADER_FLAGS=-O3 -ffast-math -I .
SHADER_FILENAMES=$(foreach f,${SHADER_NAMES},data/shaders/${f}.metal)
SHADER_IRS=$(foreach f,${SHADER_NAMES},${SHADERS_IRS_PATH}/${f}.ir) 

${SHADERS_IRS_PATH}/%.ir : data/shaders/%.metal
	@echo Shader $<
	@mkdir -p ${SHADERS_IRS_PATH}
	@xcrun -sdk macosx metal -o $@ -c $< ${SHADER_FLAGS}

${SHADER_LIB} : ${SHADER_IRS} 
	@echo Shader Library $<
	@xcrun -sdk macosx metallib -o ${SHADER_LIB} $+

shaders : ${SHADER_LIB} 
	@echo All shaders compiled

clean :
	rm -rf objs
	rm -f ${ROOT_APP_NAME}*

help :
	@echo "  make -j                         # Build OSX"
	@echo "  make RELEASE=1 -j               # Build OSX in shipping"

# osx :
# 	@echo Building for OSX
# 	@make --no-print-directory PLATFORM=OSX -j -r

universal_osx :
	@make --no-print-directory PLATFORM=OSX -j -r
	@make --no-print-directory PLATFORM=ARM -j -r
	@echo Creating Universal Binary
	@lipo -create -output ${ROOT_APP_NAME}_universal ${ROOT_APP_NAME}_OSX ${ROOT_APP_NAME}_ARM

.phony : clean all tools icons osx ios assets

