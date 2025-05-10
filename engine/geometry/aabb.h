#ifndef INC_AABB_H_
#define INC_AABB_H_

#include "geometry.h"

struct ENGINE_API TAABB {
  VEC3 center;
  VEC3 half;

  // Model * ( center +/- halfsize ) = model * center + model * half_size
  TAABB getRotatedBy(const MAT44& model) const {
    TAABB new_aabb;
    new_aabb.center = model.transformCoord(center);
    new_aabb.half.x = half.x * fabsf(model(0, 0))
      + half.y * fabsf(model(1, 0))
      + half.z * fabsf(model(2, 0));
    new_aabb.half.y = half.x * fabsf(model(0, 1))
      + half.y * fabsf(model(1, 1))
      + half.z * fabsf(model(2, 1));
    new_aabb.half.z = half.x * fabsf(model(0, 2))
      + half.y * fabsf(model(1, 2))
      + half.z * fabsf(model(2, 2));
    return new_aabb;
  }

  TAABB() {}
  TAABB(const VEC3 acenter, const VEC3 ahalf) : center(acenter), half(ahalf) { }
  TAABB(const TAABB aabb, const MAT44& model) {
    center = model.transformCoord(aabb.center);
    half.x = aabb.half.x * fabsf(model(0, 0))
      + aabb.half.y * fabsf(model(1, 0))
      + aabb.half.z * fabsf(model(2, 0));
    half.y = aabb.half.x * fabsf(model(0, 1))
      + aabb.half.y * fabsf(model(1, 1))
      + aabb.half.z * fabsf(model(2, 1));
    half.z = aabb.half.x * fabsf(model(0, 2))
      + aabb.half.y * fabsf(model(1, 2))
      + aabb.half.z * fabsf(model(2, 2));
  }

  void setMinMax(const VEC3& pmin, const VEC3& pmax) {
    center = (pmin + pmax) * 0.5f;
    half = (pmax - pmin) * 0.5f;
    half.x = fabsf(half.x);
    half.y = fabsf(half.y);
    half.z = fabsf(half.z);
  }
  VEC3 getMinCorner() const { return center - half; }
  VEC3 getMaxCorner() const { return center + half; }
  bool isEmpty() const {
    return half.lengthSquared() == 0.f;
  }
  bool intersects(VEC3 Origin, VEC3 Direction, float& Dist, float* tOut = nullptr) const;
  void enlarge(const TAABB& aabb) {
    VEC3 new_min = VEC3::Min(getMinCorner(), aabb.getMinCorner());
    VEC3 new_max = VEC3::Max(getMaxCorner(), aabb.getMaxCorner());
    setMinMax(new_min, new_max);
  }
  MAT44 asMatrix() const {
    return MAT44::createScale(half * 2.0f) * MAT44::createTranslation(center);
  }
};

#endif

