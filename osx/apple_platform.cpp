#include "platform.h"
#include "apple_platform.h"
#import <os/log.h>

#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#include "Metal/Metal.hpp"
#include "QuartzCore/CAMetalDrawable.hpp"

#include "render/render.h"
//#include "input/input.h"
#include "modules/module_render.h"

#include "imgui/imgui_impl_metal.h"

#if defined( ENABLE_PROFILING )
#error profiling is enabled!!
#endif

#if defined( IN_PLATFORM_WINDOWS )
#error in windows paltform
#endif

static ModuleRender* render_module = nullptr;

// void inputSend( const char* event_name, float x, float y ) {
//   Input::get().send( event_name, x, y );
// }

VEC2 mouse_cursor;

static int counter = 0;
void osSysLog( const char* msg ) {
  printf( "Hi %s", msg);
  os_log(OS_LOG_DEFAULT, "App.Log[%d] %{public}s", counter++, msg );
}

void renderOpen() {

  // CFBundleRef main_bundle = CFBundleGetMainBundle();
  // CFURLRef url_ref = CFBundleCopyResourcesDirectoryURL(main_bundle);
  // uint8_t buf[2048] = { };
  // if( CFURLGetFileSystemRepresentation(url_ref, true, buf, 2047) )
  //   strcpy( TBuffer::root_path, (char*)buf );
  
#if IN_PLATFORM_IOS
  setFatalHandler( &osSysLog );
  setDebugHandler( &osSysLog );
#endif
  
  // CFRelease( url_ref );
  
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

  static uint32_t frame_id = 0;
  RenderPlatform::beginFrame( ++frame_id, passDescriptor, commandBuffer, drawable );

  // Start the Dear ImGui frame
  // ImGui_ImplMetal_NewFrame(renderPassDescriptor);
  // ImGui_ImplOSX_NewFrame(view);
  // ImGui::NewFrame();

  render_module->generateFrame( args->width, args->height );

  // // Rendering
  // ImGui::Render();
  // ImDrawData* draw_data = ImGui::GetDrawData();

  // renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
  // id <MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
  // [renderEncoder pushDebugGroup:@"Dear ImGui rendering"];
  // ImGui_ImplMetal_RenderDrawData(draw_data, commandBuffer, renderEncoder);
  // [renderEncoder popDebugGroup];
  // [renderEncoder endEncoding];  
}
