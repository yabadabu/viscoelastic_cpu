#include "platform.h"
#include "imgui_dragex.h"

namespace ImGui {

static float pushFloatSensibilityMenu(const char* title, float initial_sensibility) {
  ImGui::PushID(title);
  ImGuiStorage* s = ImGui::GetStateStorage();
  float sensibility = s->GetFloat(ImGui::GetID("sensibility"), initial_sensibility);
  return sensibility;
}

static void popFloatSensibilityMenu(float sensibility) {
  bool mouse_becomes_down = ImGui::IsMouseClicked(2);
  if (ImGui::IsItemHovered()) {
    if (mouse_becomes_down)
      ImGui::OpenPopup("dragfloatex_options");
  }
  if (ImGui::BeginPopup("dragfloatex_options")) {
    if (ImGui::MenuItem("0.001", "", sensibility == 0.001f))
      sensibility = 0.001f;
    if (ImGui::MenuItem("0.01", "", sensibility == 0.01f))
      sensibility = 0.01f;
    if (ImGui::MenuItem("0.1", "", sensibility == 0.1f))
      sensibility = 0.1f;
    if (ImGui::MenuItem("1", "", sensibility == 1.0f))
      sensibility = 1.0f;
    if (ImGui::MenuItem("10", "", sensibility == 10.0f))
      sensibility = 10.0f;
    if (ImGui::MenuItem("100", "", sensibility == 100.0f))
      sensibility = 100.0f;
    if (ImGui::MenuItem("1000", "", sensibility == 1000.0f))
      sensibility = 1000.0f;
    ImGui::EndPopup();
  }
  ImGuiStorage* s = ImGui::GetStateStorage();
  s->SetFloat(ImGui::GetID("sensibility"), sensibility);
  ImGui::PopID();
}

bool DragFloatEx(const char* title, float* addr_value, float initial_sensibility, float v_min, float v_max) {
  float sensibility = pushFloatSensibilityMenu( title, initial_sensibility);
  bool changed = ImGui::DragFloat(title, addr_value, sensibility, v_min, v_max);
  popFloatSensibilityMenu(sensibility);
  return changed;
}

bool DragFloat2Ex(const char* title, float v[2], float initial_sensibility, float v_min, float v_max) {
  float sensibility = pushFloatSensibilityMenu(title, initial_sensibility);
  bool changed = ImGui::DragFloat2(title, v, sensibility, v_min, v_max);
  popFloatSensibilityMenu(sensibility);
  return changed;
}

bool DragFloat3Ex(const char* title, float v[3], float initial_sensibility, float v_min, float v_max) {
  float sensibility = pushFloatSensibilityMenu(title, initial_sensibility);
  bool changed = ImGui::DragFloat3(title, v, sensibility, v_min, v_max);
  popFloatSensibilityMenu(sensibility);
  return changed;
}

bool DragFloat4Ex(const char* title, float v[4], float initial_sensibility, float v_min, float v_max) {
  float sensibility = pushFloatSensibilityMenu(title, initial_sensibility);
  bool changed = ImGui::DragFloat4(title, v, sensibility, v_min, v_max);
  popFloatSensibilityMenu(sensibility);
  return changed;
}

bool DragIntEx(const char* title, int* addr_value, float speed, int v_min, int v_max) {
  ImGui::PushID(title);
  ImGuiStorage* s = ImGui::GetStateStorage();
  int sensibility = s->GetInt(ImGui::GetID("sensibility"), 1);
  bool changed = ImGui::DragInt(title, addr_value, sensibility * speed, v_min, v_max);
  bool mouse_becomes_down = ImGui::IsMouseClicked(2);
  if (ImGui::IsItemHovered()) {
    if (mouse_becomes_down)
      ImGui::OpenPopup("dragintex_options");
  }
  if (ImGui::BeginPopup("dragintex_options")) {
    if (ImGui::MenuItem("1", "", sensibility == 1))
      sensibility = 1;
    if (ImGui::MenuItem("10", "", sensibility == 10))
      sensibility = 10;
    if (ImGui::MenuItem("100", "", sensibility == 100))
      sensibility = 100;
    if (ImGui::MenuItem("1000", "", sensibility == 1000))
      sensibility = 1000;
    ImGui::EndPopup();
  }
  s->SetInt(ImGui::GetID("sensibility"), sensibility);
  ImGui::PopID();
  return changed;
}

}
