#pragma once

// this is meant to be used by plugins

// dumpbin /EXPORTS tubewindows.lib > z.txt
// We need to let the compiler know these functions exists
//#define IMPORT_ENGINE_TYPE(_type) template<> \
//  meta::internal::type_node * meta::internal::info_node< _type >::resolve() noexcept;
#define IMPORT_ENGINE_TYPE(_type) template<> \
  meta::Type* meta::resolve<_type>() noexcept;

#include "geometry/sdf/sdf.h"
#include "time/sequencer/sequencer_editor.h"
#include "../experiments/expressions/expressions.h"
#include "components/comp_camera.h"

// From tube
#ifdef PLATFORM_WINDOWS

#pragma warning(disable:4506)

IMPORT_ENGINE_TYPE(bool);
IMPORT_ENGINE_TYPE(double);
IMPORT_ENGINE_TYPE(float);
IMPORT_ENGINE_TYPE(int);
IMPORT_ENGINE_TYPE(uint32_t);
IMPORT_ENGINE_TYPE(std::string);
IMPORT_ENGINE_TYPE(VEC2);
IMPORT_ENGINE_TYPE(VEC3);
IMPORT_ENGINE_TYPE(VEC4);
IMPORT_ENGINE_TYPE(QUAT);
IMPORT_ENGINE_TYPE(TAABB);
IMPORT_ENGINE_TYPE(TStr32);
IMPORT_ENGINE_TYPE(TStr64);
IMPORT_ENGINE_TYPE(TStr128);
IMPORT_ENGINE_TYPE(TStr256);
IMPORT_ENGINE_TYPE(TTransform);
IMPORT_ENGINE_TYPE(TAABB);
IMPORT_ENGINE_TYPE(ImGuiIntParams);
IMPORT_ENGINE_TYPE(ImGuiFloatParams);
IMPORT_ENGINE_TYPE(makeImGuiColor);
IMPORT_ENGINE_TYPE(makeShowTransformHandle);
IMPORT_ENGINE_TYPE(VisibleCondition);
IMPORT_ENGINE_TYPE(OnMemberUpdated);
IMPORT_ENGINE_TYPE(meta::jsonIO);
IMPORT_ENGINE_TYPE(meta::imguiIO);
IMPORT_ENGINE_TYPE(meta::DataResolver);
IMPORT_ENGINE_TYPE(std::vector<VEC3>);
IMPORT_ENGINE_TYPE(SDF::sdFunc);
IMPORT_ENGINE_TYPE(SDF::Image);
IMPORT_ENGINE_TYPE(Sequencer::Editor);
IMPORT_ENGINE_TYPE(Sequencer::RunTime);
IMPORT_ENGINE_TYPE(Sequencer::StaticData);
IMPORT_ENGINE_TYPE(Expressions::ExpressionSlots);
IMPORT_ENGINE_TYPE(TCompCamera);
IMPORT_ENGINE_TYPE(TCompCamera::eMode);
IMPORT_ENGINE_TYPE(TCompTransform);
IMPORT_ENGINE_TYPE(CEntity);

namespace Render {
  struct TTexture;
}
IMPORT_ENGINE_TYPE(Render::TTexture const*);

#else

#define LINK_LIBRARY(x)

#endif

// Needed in case you are in a plugin and the bus is specific of the plugin
#define DECLARE_PLUGIN_MSG_BUS(T) \
  template<> \
  jaba::bus::internal::TMsgBus<T>& jaba::bus::internal::getBus() { \
    static jaba::bus::internal::TMsgBus<T> bus; \
    return bus; \
    } \

#define DECLARE_PLUGIN(module_type) \
  extern "C"    \
  __declspec(dllexport) TModule* getModule() {  \
    static module_type local_module##module_type;  \
    return &local_module##module_type;          \
  }
