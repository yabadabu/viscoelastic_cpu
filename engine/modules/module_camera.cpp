#include "platform.h"
#include "render/render.h"

static CCamera                 camera;
CCamera& getMainCamera3D() {
  return camera;
}

//struct OrbitController : public CameraController {
//  float yaw = 0.0f;
//  float pitch = 0.0f;
//  float radius = 5.0f;
//  float max_pitch = deg2rad(89.95f);
//  float move_sensitivity = 5.0f;
//  float rotate_sensitivity = 0.5f;
//  OrbitController() {
//    yaw = deg2rad(30.0f);
//    pitch = -deg2rad(30.0f);
//  }
//  VEC3 getOrigin(VEC3 target) {
//    VEC3 delta = getVectorFromYawAndPitch(yaw, pitch);
//    VEC3 origin = target - delta * radius;
//    return origin;
//  }
//  void addDeltas(float dyaw, float dpitch) {
//    yaw += dyaw;
//    pitch += dpitch;
//    if (pitch > max_pitch) pitch = max_pitch;
//    else if (pitch < -max_pitch) pitch = -max_pitch;
//  }
//  void updateCamera(CCamera* camera, float dt) override {
//    Input& input = Input::get();
//    VEC3 front = camera->getFront();
//    VEC3 target = camera->getTarget();
//    VEC3 left = camera->getLeft();
//
//    float amount_moved = move_sensitivity * dt;
//    float amount_rotated = rotate_sensitivity * dt;
//
//    float amount_fwd = input["forward"];
//    float amount_left = input["left"];
//
//    target += left * amount_moved * amount_left;
//    target += front * amount_moved * amount_fwd;
//
//    if (input.drag.isDeltaNonZero())
//      addDeltas(-input.drag.x.delta() * amount_rotated, -input.drag.y.delta() * amount_rotated);
//
//    camera->lookAt(getOrigin(target), target, VEC3::axis_y);
//  }
//};

struct FlyController : public CameraController {
  float yaw = 0.0f;
  float pitch = 0.0f;
  float max_pitch = deg2rad(89.95f);
  float move_sensitivity = 5.0f;
  FlyController() {
    yaw = deg2rad(30.0f);
    pitch = -deg2rad(30.0f);
  }
  void addDeltas(float dyaw, float dpitch) {
    yaw += dyaw;
    pitch += dpitch;
    if (pitch > max_pitch) pitch = max_pitch;
    else if (pitch < -max_pitch) pitch = -max_pitch;
  }
  void updateCamera(CCamera* camera, float dt) override {
    //ImGui::GetIO().iskeIsKeyDown()
    float rotate_yaw = 0.0f;
    // 
    // 
    //Input& input = Input::get();
    VEC3 pos = camera->getPosition();
    VEC3 left = camera->getLeft();
    VEC3 front = camera->getFront();

    getYawPitchFromVector(front, &yaw, &pitch);

    float amount_moved = move_sensitivity * dt;
    float amount_rotated = dt;

    bool camera_boost = false; //input["camera.boost"]
    bool camera_slow = false; // input["camera.slow"];

    if (camera_boost)
      amount_moved *= 4.0f; // input["camera.boost"].value;
    if (camera_slow)
      amount_moved *= 0.1f; // input["camera.slow"].value;

    float amount_fwd = 0.0f; //input["forward"];
    float amount_left = 0.0f; //input["left"];

    pos += front * amount_moved * amount_fwd;
    pos += left * amount_moved * amount_left;

    // Rotation
    float amount_yaw = 0.0f; // input["rotate_yaw"];
    float amount_pitch = 0.0f; // input["rotate_pitch"];
    addDeltas(amount_yaw * amount_rotated, amount_pitch * amount_rotated);

    VEC3 new_front = getVectorFromYawAndPitch(yaw, pitch);
    camera->lookAt(pos, pos + new_front, VEC3::axis_y);
  }
};

struct ModuleCamera : public Module {
  const char* getName() const override { return "camera"; }
  bool startsLoaded() const override { return true; }
  int getPriority() const override { return 14; }

  FlyController fly;
  //OrbitController orbit;
  CameraController* controller = nullptr;

  void load() override {
    //subscribe(this, &ModuleCamera::onAppMsgSetMainCameraController);
    controller = &fly;
  }

  //void onAppMsgSetMainCameraController() {
  //  CameraController* new_controller = nullptr;
  //  if (msg.controller) {
  //    new_controller = msg.controller;
  //  }
  //  else if (TStr16("orbit") == msg.name) {
  //    new_controller = &orbit;
  //  }
  //  else {
  //    new_controller = &fly;
  //  }

  //  if (new_controller != controller) {
  //    if (controller)
  //      controller->onControllerDeActivated();
  //    controller = new_controller;
  //    if( controller )
  //      controller->onControllerActivated();
  //  }

  //}

  void update() {
    float dt = 1.0f / 30.0f;
    controller->updateCamera(Render::getCurrentRenderCamera(), dt);
  }

};

ModuleCamera module_camera;