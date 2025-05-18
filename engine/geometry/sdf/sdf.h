#pragma once

namespace SDF {

  float sdSphere(VEC3 p, float s);
  float sdBox(VEC3 p, VEC3 radius, float softness);
  float sdPlane(VEC3 p, VEC3 n, float h);

  struct Primitive {
    enum class eType {
      SPHERE,
      BOX,
      PLANE
    };
    MAT44      to_local;
    VEC4       color = VEC4(1, 1, 1, 1);
    TTransform transform;
    bool       enabled = true;
    eType      prim_type = eType::SPHERE;
    float      softness = 0.0f;
    float      multiplier = 1.0f;

    float eval(VEC3 p) const;

    void transformHasChanged();
    static Primitive makeSphere(VEC3 center, float rad);
    static Primitive makeBox(VEC3 center, VEC3 sizes);
    static Primitive makePlane(VEC3 center, VEC3 normal);

    bool renderInMenu();
  };

  struct sdFunc {
    std::vector< Primitive > prims;
    float eval(VEC3 p) const;
    VEC3  evalGrad(VEC3 p) const;
    void renderWire() const;
    uint32_t countPrimitives(Primitive::eType type) const;

    template< typename Fn >
    void onEachPrimitive(Primitive::eType prim_type, Fn fn) const {
      for (auto& p : prims) {
        if (p.enabled && p.prim_type == prim_type)
          fn(p);
      }
    }

    // -------------------------------------------------
    // n*r + d = 0;
    struct Plane {
      VEC3  n;
      float d = 0.0f;
      float multiplier = 1.0f;
      Plane() = default;
      Plane(VEC3 new_n, float new_d) : n(new_n.normalized()), d(new_d) {}
      void fromTransform(const TTransform& t) {
        n = t.getUp().normalized();
        d = -n.dot(t.getPosition());
      }
    };
    std::vector< Plane > planes;

    // -------------------------------------------------
    struct Sphere {
      VEC3  c;
      float r = 0.0f;
      float multiplier = 1.0f;
      Sphere() = default;
      Sphere(VEC3 new_c, float new_r, float new_multiplier) : c(new_c), r(new_r), multiplier(new_multiplier) {}
      void fromTransform(const TTransform& t) {
        c = t.getPosition();
        r = t.getScale().x;
      }
    };
    std::vector< Sphere > spheres;

    // -------------------------------------------------
    struct OrientedBox {
      MAT44 to_local;
      float radius = 1.0f;
      float softness = 1.0f;
      float multiplier = 1.0f;
      float padding = 0.0f;
      OrientedBox() = default;
    };
    std::vector< OrientedBox > oriented_boxes;

    // -------------------------------------------------
    void generateCompactStructs();
    float evalCompact(VEC3 p) const;
    VEC3  evalGradCompact(VEC3 p) const;
    
    bool renderInMenu();
  };

  enum class eSide { INTERIOR, CONTOUR, EXTERIOR };

  struct Image;
}

