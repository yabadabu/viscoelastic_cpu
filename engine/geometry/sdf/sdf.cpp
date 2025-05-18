#include "platform.h"
#include "sdf.h"
#include "render/render.h"

namespace SDF {

  // --------------------------------------------------------------------------
  float sdSphere(VEC3 p, float s) {
    return p.length() - s;
  }

  float sdPlane(VEC3 p, VEC3 n, float h) {
    return n.dot(p) + h;
  }

  // As the plane can be rotated and translated, we use the plane y = 0
  float sdPlaneXZ(VEC3 p) {
    return p.y;
  }

  // --------------------------------------------------------------------------
// Signed box centered in the origin
  float sdBox(VEC3 p, VEC3 radius, float softness) {
    VEC3 d = p.abs() - radius;
    float m = d.maxComponent();
    return (VEC3::Max(d, VEC3::zero)).length() + std::min(m, 0.0f) - softness;
  }

  // --------------------------------------------------------------------------
  Primitive Primitive::makeSphere(VEC3 center, float rad) {
    Primitive p;
    p.transform.setPosition(center);
    p.transform.setScale(VEC3::ones * rad);
    p.prim_type = eType::SPHERE;
    p.transformHasChanged();
    return p;
  }

  Primitive Primitive::makeBox(VEC3 center, VEC3 sizes) {
    Primitive p;
    p.transform.setPosition(center);
    p.transform.setScale(sizes * 2.0f);
    p.prim_type = eType::BOX;
    p.transformHasChanged();
    return p;
  }

  Primitive Primitive::makePlane(VEC3 center, VEC3 normal) {
    Primitive p;
    p.transform.setPosition(center);
    p.transform.setRotation(QUAT::createFromDirectionToDirection(VEC3::axis_y, normal));
    p.prim_type = eType::PLANE;
    p.transformHasChanged();
    return p;
  }

  // --------------------------------------------------------------------------
  float Primitive::eval(VEC3 p) const {
    p = to_local.transformCoord(p);
    if (prim_type == eType::SPHERE)
      return SDF::sdSphere(p, 1.0f) * multiplier;
    else if (prim_type == eType::PLANE)
      return SDF::sdPlaneXZ(p) * multiplier;
    else if (prim_type == eType::BOX) {
      constexpr float r = 0.5f;
      return SDF::sdBox(p, VEC3(r, r, r), softness) * multiplier;
    }
    return 1.0f;
  }

  void Primitive::transformHasChanged() {
    to_local = transform.asMatrix().inverse();
  }

  float sdFunc::eval(VEC3 p) const {
    float dmin = FLT_MAX;
    for (auto& prim : prims) {
      if (!prim.enabled)
        continue;
      float dnew = prim.eval(p);
      dmin = std::min(dmin, dnew);
    }
    return dmin;
  }

  float sdFunc::evalCompact(VEC3 p) const {
    float dmin = FLT_MAX;
    for (auto& prim : planes)
      dmin = std::min(dmin, sdPlane(p, prim.n, prim.d) * prim.multiplier);
    for (auto& prim : spheres)
      dmin = std::min(dmin, sdSphere(p - prim.c, prim.r) * prim.multiplier);
    
    const VEC3 half_unit(0.5f, 0.5f, 0.5f);
    for (auto& prim : oriented_boxes) {
      VEC3 local_p = prim.to_local.transformCoord(p);
      dmin = std::min(dmin, sdBox(local_p, half_unit, prim.softness)* prim.multiplier );
    }
    return dmin;
  }

  VEC3 sdFunc::evalGradCompact(VEC3 p) const {
    const float eps = 0.01f;
    return VEC3(
      evalCompact(VEC3(p.x + eps, p.y, p.z)) - evalCompact(VEC3(p.x - eps, p.y, p.z)),
      evalCompact(VEC3(p.x, p.y + eps, p.z)) - evalCompact(VEC3(p.x, p.y - eps, p.z)),
      evalCompact(VEC3(p.x, p.y, p.z + eps)) - evalCompact(VEC3(p.x, p.y, p.z - eps))
    ).normalized();
  }

  VEC3 sdFunc::evalGrad(VEC3 p) const {
    const float eps = 0.01f;
    //if (use_central_differences_for_grad)
    return VEC3(
      eval(VEC3(p.x + eps, p.y, p.z)) - eval(VEC3(p.x - eps, p.y, p.z)),
      eval(VEC3(p.x, p.y + eps, p.z)) - eval(VEC3(p.x, p.y - eps, p.z)),
      eval(VEC3(p.x, p.y, p.z + eps)) - eval(VEC3(p.x, p.y, p.z - eps))
    ).normalized();
  }

  void sdFunc::renderWire() const {
    for (auto& p : prims) {
      if (!p.enabled)
        continue;
      if (p.prim_type == Primitive::eType::SPHERE) {
        const Render::Mesh* circle = Resource<Render::Mesh>("unit_circle_xz.mesh");
        MAT44 world = p.transform.asMatrix();
        Render::drawPrimitive(circle, world, Color::Yellow);
        world = MAT44::createFromAxisAngle(VEC3::axis_x, deg2rad(90.0f)) * world;
        Render::drawPrimitive(circle, world, Color::Yellow);
        world = MAT44::createFromAxisAngle(VEC3::axis_z, deg2rad(90.0f)) * world;
        Render::drawPrimitive(circle, world, Color::Yellow);
      }
      else if (p.prim_type == Primitive::eType::PLANE) {
        const Render::Mesh* grid = Resource<Render::Mesh>("grid.mesh");
        Render::drawPrimitive(grid, MAT44::createScale(0.025f) * p.transform.asMatrix(), p.color);
      }
      else if (p.prim_type == Primitive::eType::BOX) {
        const Render::Mesh* mesh = Resource<Render::Mesh>("unit_wired_cube.mesh");
        Render::drawPrimitive(mesh, p.transform.asMatrix(), p.color);
      }
      else {
        fatal("Render SDF prim type %d not supported\n", (int)p.prim_type);
      }
    }
  }

  uint32_t sdFunc::countPrimitives(Primitive::eType type) const {
    uint32_t n = 0;
    for (auto& p : prims) {
      if (p.enabled && p.prim_type == type)
        ++n;
    }
    return n;
  }

  void sdFunc::generateCompactStructs() {
    planes.clear();
    onEachPrimitive(SDF::Primitive::eType::PLANE, [&](const SDF::Primitive& p) {
      Plane plane;
      plane.fromTransform(p.transform);
      plane.multiplier = p.multiplier;
      planes.emplace_back(plane);
      });

    spheres.clear();
    onEachPrimitive(SDF::Primitive::eType::SPHERE, [&](const SDF::Primitive& p) {
      VEC3 center = p.transform.getPosition();
      float radius = p.transform.getScale().x;
      spheres.emplace_back(center, radius, p.multiplier);
      });

    oriented_boxes.clear();
    onEachPrimitive(SDF::Primitive::eType::BOX, [&](const SDF::Primitive& p) {
      OrientedBox obox;
      obox.to_local = p.to_local;
      obox.radius = 1.0f;
      obox.softness = p.softness;
      obox.multiplier = p.multiplier;
      oriented_boxes.push_back(obox);
      });

  }

  bool Primitive::renderInMenu() {
    ImGui::PushID(this);
    bool changed = false;

    int itype = (int)prim_type;
    if (ImGui::Combo("Type", &itype, "Sphere\0Box\0Plane\0\0", 4)) {
      prim_type = (eType)(itype);
      changed = true;
    }

    changed |= ImGui::Checkbox("Enabled", &enabled);
    changed |= ImGui::DragFloat("Weight", &multiplier, 0.1f, -5.0f, 5.0f );
    if (ImGui::TreeNode("Transform...")) {
      if (transform.debugInMenu())
        transformHasChanged();
      ImGui::TreePop();
    }
    ImGui::PopID();
    return changed;
  }

  bool sdFunc::renderInMenu() {
    bool changed = false;
    for (auto& prim : prims) {
      ImGui::Separator();
      changed |= prim.renderInMenu();
    }
    if (ImGui::SmallButton("Add..."))
      prims.push_back(SDF::Primitive::makeSphere(VEC3(0, 0, 0), 1.0f));

    return changed;
  }
}
