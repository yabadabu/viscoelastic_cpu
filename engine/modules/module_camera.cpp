#include "platform.h"
#include "render/render.h"

static CCamera                 camera;
CCamera& getMainCamera3D() {
  return camera;
}

struct FlyController : public CameraController {
  float yaw = 0.0f;
  float pitch = 0.0f;
  float max_pitch = deg2rad(89.95f);
  float move_sensitivity = 4.0f;
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
    VEC3 pos = camera->getPosition();
    VEC3 left = camera->getLeft();
    VEC3 front = camera->getFront();

    getYawPitchFromVector(front, &yaw, &pitch);

    float amount_moved = move_sensitivity * dt;
    float amount_rotated = dt;

    bool camera_boost = false;
    bool camera_slow = false;

    if (camera_boost)
      amount_moved *= 4.0f;
    if (camera_slow)
      amount_moved *= 0.1f;

    float amount_fwd = ImGui::IsKeyDown(ImGuiKey_W) ? 1.0f : (ImGui::IsKeyDown(ImGuiKey_S) ? -1.0f : 0.0);
    float amount_left = ImGui::IsKeyDown(ImGuiKey_A) ? 1.0f : (ImGui::IsKeyDown(ImGuiKey_D) ? -1.0f : 0.0);

    pos += front * amount_moved * amount_fwd;
    pos += left * amount_moved * amount_left;

    // Rotation
    float amount_yaw = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right).x * (-0.5f);
    float amount_pitch = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right).y * (-0.5f);
    ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);
    addDeltas(amount_yaw * amount_rotated, amount_pitch * amount_rotated);

    VEC3 new_front = getVectorFromYawAndPitch(yaw, pitch);
    camera->lookAt(pos, pos + new_front, VEC3::axis_y);
  }
};

struct ModuleCamera : public Module {
  const char* getName() const override { return "camera"; }
  int getPriority() const override { return 14; }

  FlyController     fly;
  CameraController* controller = nullptr;

  void load() override {
    controller = &fly;
  }

  void update() {
    float dt = frameTime().elapsed;
    controller->updateCamera(Render::getCurrentRenderCamera(), dt);
  }

};

ModuleCamera module_camera;