#include "platform.h"
#include "render/render.h"
#include "module_render.h"

// Render globals
Render::TypedBuffer<CteCamera> cte_camera;
Render::TypedBuffer<CteObject> cte_object;
static constexpr uint32_t max_world_objects = 64;
static MakeID used_slots(max_world_objects);

CCamera                        g_camera;

namespace Render {
  CCamera* getCurrentRenderCamera() {
    return &g_camera;
  }
}

CteObject* getCteObjectsData(uint32_t index) {
  assert(index < cte_object.num_instances);
  return cte_object.data() + index;
}

uint32_t allocCteObjects(uint32_t count) {
  uint32_t first = -1;
  bool is_ok = used_slots.CreateRangeID(first, count);
  if (!is_ok)
    fatal("Not enough slots in the object's shader cte for %d new entries\n",
      count);
  return first;
}

void freeCteObjects(uint32_t first, uint32_t count) {
  used_slots.DestroyRangeID(first, count);
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
  is_ok &= cte_object.create(ShadersSlotObjects, "Objects", max_world_objects);
  (void)is_ok;
  assert(is_ok);
}

void ModuleRender::unload() {
  cte_object.destroy();
  cte_camera.destroy();
}

void ModuleRender::generateFrame(int w, int h) {
  assert(w > 0 && h > 0);

  Render::frameStarts();

  RenderPlatform::beginRenderingBackBuffer();
  activateCamera(Render::getCurrentRenderCamera(), w, h);

  Modules::get().onRenderDebug3D();

  //renderCategory(camera, Render::CategorySolids, "Solids");
  //renderCategory(camera, Render::CategoryPostFX, "FX");
  //renderCategory(camera, Render::CategoryDebug3D, "Debug3D");
  //renderCategory(camera, Render::CategoryUI, "UI");

  RenderPlatform::endRenderingBackBuffer();
}

ModuleRender module_render;
