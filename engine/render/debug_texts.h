#pragma once

#include "geometry/camera.h"

class CDebugTexts {
  struct Text3D {
    char     str[64] = { 0x00 };
    VEC3     p;
    uint32_t color = 0xffffffff;
  };
  std::vector< Text3D > texts;
public:
  void add(VEC3 p, uint32_t color, const char* fmt, ...) {
    texts.resize(texts.size() + 1);
    Text3D& b = texts.back();
    b.p = p;
    b.color = color;
    va_list argp;
    va_start(argp, fmt);
    int n = vsnprintf(b.str, sizeof(b.str) - 1, fmt, argp);
    va_end(argp);
  }
  void flush() {
    CCamera* camera = Render::getCurrentRenderCamera();
    if (!camera)
      return;

    if (ImGui::Begin("gizmo"))
    {
      float px = 1.0f;
      const TViewport& vp = camera->getViewport();
      auto* imgui_vp = ImGui::GetMainViewport();
      const float cx0 = vp.TopLeftX + imgui_vp->Pos.x;
      const float cy0 = vp.TopLeftY + imgui_vp->Pos.y;
      auto* dl = ImGui::GetBackgroundDrawList();
      for (auto& m : texts) {

        // Is the point visible in the screen?
        ImVec2 center;
        if (!camera->getScreenCoords(m.p, &center.x, &center.y))
          continue;
        center.x += cx0;
        center.y += cy0;
        
#ifdef PLATFORM_APPLE
        ImGuiIO& io = ImGui::GetIO();
        center.x /= io.DisplayFramebufferScale.x;
        center.y /= io.DisplayFramebufferScale.y;
#endif

        // Center text
        ImVec2 sz = ImGui::CalcTextSize(m.str);
        ImVec2 pmin(center.x - sz.x * 0.5f, center.y - sz.y * 0.5f);
        ImVec2 pmax(center.x + sz.x * 0.5f, center.y + sz.y * 0.5f);
        ImVec2 text_pos = pmin;
        pmin.x -= px + 2.0f;
        pmin.y -= px;
        pmax.x += px + 2.0f;
        pmax.y += px;
        dl->AddRectFilled(pmin, pmax, 0xa0000000, 2.f);
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), text_pos, m.color, m.str);
      }
    }
    ImGui::End();

    texts.clear();
  }
};
