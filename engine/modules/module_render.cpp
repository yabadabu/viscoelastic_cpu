#include "platform.h"
#include "render/render.h"
#include "module_render.h"

// Render globals
Render::TypedBuffer<CteCamera> cte_camera;
Render::TypedBuffer<CteObject> cte_object;

CCamera                        g_camera;

namespace Render {
  CCamera* getCurrentRenderCamera() {
    return &g_camera;
  }
}

static void activateCamera(CCamera* cam, int w, int h) {
  cam->setViewport(0, 0, (int)w, (int)h);
  CteCamera* cc = cte_camera.data(0);
  cc->view_proj = cam->getViewProjection();
  cc->position = cam->getPosition();
  cc->left = cam->getLeft();
  cc->forward = cam->getFront();
  cc->aspect_ratio = cam->getAspectRatio();
  cc->up = cam->getUp();
  cc->world_time = (float)frameTime().now;
  Render::Encoder* encoder = Render::getMainEncoder();
  encoder->setVertexBuffer(&cte_camera);
  encoder->setVertexBufferOffset(0, sizeof(CteCamera), ShadersSlotCamera);
}

void ModuleRender::load() {

  CCamera* camera = Render::getCurrentRenderCamera();
  camera->setProjectionParams(deg2rad(80), 0.1f, 100.0f);
  camera->lookAt(VEC3(5, 3, 2), VEC3(0, 0, 0), VEC3(0, 1, 0));

  bool is_ok = true;
  is_ok &= cte_camera.create(ShadersSlotCamera, "Camera", 1);
  (void)is_ok;
  assert(is_ok);
}

void ModuleRender::renderInMenu() {
  if (ImGui::SmallButton("Profile Capture"))
    PROFILE_START_CAPTURING(5);
}

void ModuleRender::unload() {
  cte_camera.destroy();
}

void ModuleRender::generateFrame(int w, int h) {
  assert(w > 0 && h > 0);

  RenderPlatform::beginRenderingBackBuffer();
  activateCamera(Render::getCurrentRenderCamera(), w, h);
  
  Modules::get().onRender3D();
  Modules::get().renderInMenu();

  RenderPlatform::endRenderingBackBuffer();

}

ModuleRender module_render;
