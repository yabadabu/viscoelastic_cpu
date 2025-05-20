#include "platform.h"
#include "apple_platform.h"

#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#include "Metal/Metal.hpp"
#include "QuartzCore/CAMetalDrawable.hpp"

#include "render/render.h"
#include "modules/module_render.h"

#include "imgui/imgui_impl_osx.h"
#include "imgui/imgui_impl_metal.h"
#include "imgui/ImGuizmo.h"

static ModuleRender* render_module = nullptr;

VEC2 mouse_cursor;
void inputSend( const char* event_name, float x, float y ) {
  if( strcmp( event_name, "mouse.move") == 0 ) {
    mouse_cursor.x = x;
    mouse_cursor.y = y;
  }
}

void renderOpen( void* raw_view ) {
  
  PROFILE_START_CAPTURING(5);
  PROFILE_BEGIN_FRAME();
  PROFILE_SCOPED_NAMED("renderOpen");
  RenderPlatform::create();
  Modules::get().load();
  render_module = (ModuleRender*)(Modules::get().getModule("render"));
  assert(render_module);

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
  ImGui::StyleColorsDark();
  ImGui_ImplOSX_Init(raw_view);
  ImGui_ImplMetal_Init(RenderPlatform::getDevice());
}

void renderClose() {
  Modules::get().unload();
  RenderPlatform::destroy();
}

void renderFrame( RenderArgs* args ) {
  PROFILE_BEGIN_FRAME();
  PROFILE_SCOPED();
  
  Modules::get().update();

  MTL::RenderPassDescriptor* passDescriptor = (MTL::RenderPassDescriptor*)args->renderPassDescriptor;
  assert( passDescriptor );
  CA::MetalDrawable* drawable = (CA::MetalDrawable*)args->drawable;
  assert( drawable );
  MTL::CommandBuffer* commandBuffer = RenderPlatform::getCommandBuffer();

  ImGui_ImplMetal_NewFrame(passDescriptor);
  ImGui_ImplOSX_NewFrame(args->view);
  ImGui::NewFrame();
  ImGuizmo::BeginFrame();

  static uint32_t frame_id = 0;
  RenderPlatform::beginFrame( ++frame_id, passDescriptor, commandBuffer, drawable );

  render_module->generateFrame( args->width, args->height );

  // ImGui
  ImGui::Render();
  Render::Encoder* encoder = Render::getMainEncoder( );
  ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), commandBuffer, encoder->encoder);

  RenderPlatform::swapFrames();
}
