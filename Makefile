#PLATFORM ?= IOS
PLATFORM ?= OSX

ROOT_APP_NAME:=demo
APP_NAME=${ROOT_APP_NAME}_${PLATFORM}

TARGET : ${APP_NAME}

INCLUDE_PATHS=. engine engine/render/metal osx/metal-cpp
INCLUDE_OPTIONS+=$(foreach f,${INCLUDE_PATHS},-I$f)

CFLAGS=-Wall -c ${INCLUDE_OPTIONS} -DIN_PLATFORM_${PLATFORM}=1  -DIN_PLATFORM_APPLE=1 -DIMGUI_IMPL_METAL_CPP_EXTENSIONS

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

CXXFLAGS=${CFLAGS} -std=c++17 -fno-exceptions -fno-objc-arc
cooker: CXXFLAGS := ${CFLAGS} -std=c++20 -fno-objc-arc

FRAMEWORKS=Foundation Metal MetalKit AudioToolbox GameKit

ifeq (${PLATFORM}, OSX)
	FRAMEWORKS+=QuartzCore Cocoa AppKit
	SRCS=main_osx imgui_impl_metal imgui_impl_osx
	ARCH_FLAGS=-target x86_64-apple-macos14s -mavx2
	LIBS+=-F/System/Library/Frameworks
else ifeq (${PLATFORM}, ARM)
	FRAMEWORKS+=QuartzCore Cocoa AppKit
	SRCS=main_osx
	ARCH_FLAGS=-target arm64-apple-macos14
	LIBS+=-F/System/Library/Frameworks
else
	FRAMEWORKS+=UIKit CoreMotion Security CoreLocation
	SRCS=main_ios 
	SYSROOT=$(shell xcrun --show-sdk-path --sdk iphoneos)
	ARCH_FLAGS=-target arm64-apple-ios14 -isysroot ${SYSROOT}
	CXXFLAGS+=-arch arm64
	LIBS+=-lsoloud_static -lcurl -lz 
endif

LNKFLAGS+=${ARCH_FLAGS}
CFLAGS+=${ARCH_FLAGS}
CXXFLAGS+=${ARCH_FLAGS}

LIBS+=$(foreach f,${FRAMEWORKS},-framework $f)
LIBS+=-lstdc++ ${ASAN_FLAGS}

OBJS_PATH=objs/${PLATFORM}/${CONFIG_PATH}

# Get All module sources
MODULE_SRCS=$(foreach f,$(shell find engine/modules -name "*.cpp"),${notdir ${basename $f}})

SRCS+=apple_platform \
     geometry transform camera angular sdf \
     render primitives \
     json json_file \
     utils \
     resources_manager \
     render_platform \
     imgui imgui_draw imgui_widgets imgui_tables imgui_demo ImGuizmo \
     viscoelastic viscoelastic_sim \
     ${MODULE_SRCS} \

#SRCS=demo
OBJS=$(foreach f,${SRCS},$(OBJS_PATH)/$(basename $f).o)

VPATH=${shell find engine -type d| grep -v objs | grep -v common} osx experiments tools

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
	@echo "  make osx                        # Build OSX"
	@echo "  make osx RELEASE=1              # Build OSX in shipping"
	@echo "  make install                    # Build and install iOS"
	@echo "  make tools                      # Build tools for osx"
	@echo "  make assets                     # Cook assets from assets -> data"

osx :
	@make --no-print-directory PLATFORM=OSX -j -r

universal_osx :
	@make --no-print-directory PLATFORM=OSX -j -r
	@make --no-print-directory PLATFORM=ARM -j -r
	@echo Creating Universal Binary
	@lipo -create -output ${ROOT_APP_NAME}_universal ${ROOT_APP_NAME}_OSX ${ROOT_APP_NAME}_ARM

.phony : clean all tools icons osx ios assets
