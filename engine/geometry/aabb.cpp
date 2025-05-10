#include "platform.h"
#include "aabb.h"

static bool Clip(float Denom, float Numer, float& T0, float& T1) {
  // Return value is 'true' if line segment intersects the current test
  // plane.  Otherwise 'false' is returned in which case the line segment
  // is entirely clipped.
  if (Denom > 0.0f) {
    if (Numer > Denom * T1) {
      return false;
    }
    if (Numer > Denom * T0) {
      T0 = Numer / Denom;
    }
    return true;
  }
  else if (Denom < 0.0f) {
    if (Numer > Denom * T0) {
      return false;
    }
    if (Numer > Denom * T1) {
      T1 = Numer / Denom;
    }
    return true;
  }
  else return Numer <= 0.0f;
}

bool TAABB::intersects(const VEC3 line_src, const VEC3 line_dir, float& dist, float* tOut) const {
  VEC3 new_line_src = line_src - center;

  /*
  // p0 + t*v = p
  VEC3 inv_dir = VEC3(1.0f / line_dir.x, 1.0f / line_dir.y, 1.0f / line_dir.z);
  VEC3 tmin = (-half - new_line_src); tmin *= inv_dir;
  VEC3 tmax = (half - new_line_src); tmax *= inv_dir;
  if (tmin.x > tmax.x) std::swap(tmin.x, tmax.x);
  if (tmin.y > tmax.y) std::swap(tmin.y, tmax.y);
  if (tmin.z > tmax.z) std::swap(tmin.z, tmax.z);
  float t_in = std::max(std::max(tmin.x, tmin.y), tmin.z);
  float t_out = std::min(std::min(tmax.x, tmax.y), tmax.z);
  if (t_in > t_out)
    return false;
  dist = t_in;
  return true;
  */

  float T0 = 0.0f;
  float T1 = FLT_MAX;
  bool NotEntirelyClipped =
    Clip(+line_dir.x, -new_line_src.x - half.x, T0, T1) &&
    Clip(-line_dir.x, +new_line_src.x - half.x, T0, T1) &&
    Clip(+line_dir.y, -new_line_src.y - half.y, T0, T1) &&
    Clip(-line_dir.y, +new_line_src.y - half.y, T0, T1) &&
    Clip(+line_dir.z, -new_line_src.z - half.z, T0, T1) &&
    Clip(-line_dir.z, +new_line_src.z - half.z, T0, T1);
  dist = T0;
  if (tOut) *tOut = T1;
  return NotEntirelyClipped && (T0 != 0.0f || T1 != FLT_MAX);
}
/*
bool TAABB::intersects(VEC3 origin, VEC3 Direction, float& Dist) const
{

  assert(DirectX::Internal::XMVector3IsUnit(Direction));

  // Load the box.
  XMVECTOR Origin = XMLoadFloat3(&origin);
  XMVECTOR vCenter = XMLoadFloat3(&center);
  XMVECTOR vExtents = XMLoadFloat3(&half);

  // Adjust ray origin to be relative to center of the box.
  XMVECTOR TOrigin = vCenter - Origin;

  // Compute the dot product againt each axis of the box.
  // Since the axii are (1,0,0), (0,1,0), (0,0,1) no computation is necessary.
  XMVECTOR AxisdotOrigin = TOrigin;
  XMVECTOR AxisdotDirection = Direction;

  // if (fabs(AxisdotDirection) <= Epsilon) the ray is nearly parallel to the slab.
  XMVECTOR IsParallel = XMVectorLessOrEqual(XMVectorAbs(AxisdotDirection), g_RayEpsilon);

  // Test against all three axii simultaneously.
  XMVECTOR InverseAxisdotDirection = XMVectorReciprocal(AxisdotDirection);
  XMVECTOR t1 = (AxisdotOrigin - vExtents) * InverseAxisdotDirection;
  XMVECTOR t2 = (AxisdotOrigin + vExtents) * InverseAxisdotDirection;

  // Compute the max of min(t1,t2) and the min of max(t1,t2) ensuring we don't
  // use the results from any directions parallel to the slab.
  XMVECTOR t_min = XMVectorSelect(XMVectorMin(t1, t2), g_FltMin, IsParallel);
  XMVECTOR t_max = XMVectorSelect(XMVectorMax(t1, t2), g_FltMax, IsParallel);

  // t_min.x = maximum( t_min.x, t_min.y, t_min.z );
  // t_max.x = minimum( t_max.x, t_max.y, t_max.z );
  t_min = XMVectorMax(t_min, XMVectorSplatY(t_min));  // x = max(x,y)
  t_min = XMVectorMax(t_min, XMVectorSplatZ(t_min));  // x = max(max(x,y),z)
  t_max = XMVectorMin(t_max, XMVectorSplatY(t_max));  // x = min(x,y)
  t_max = XMVectorMin(t_max, XMVectorSplatZ(t_max));  // x = min(min(x,y),z)

  // if ( t_min > t_max ) return false;
  XMVECTOR NoIntersection = XMVectorGreater(XMVectorSplatX(t_min), XMVectorSplatX(t_max));

  // if ( t_max < 0.0f ) return false;
  NoIntersection = XMVectorOrInt(NoIntersection, XMVectorLess(XMVectorSplatX(t_max), XMVectorZero()));

  // if (IsParallel && (-Extents > AxisdotOrigin || Extents < AxisdotOrigin)) return false;
  XMVECTOR ParallelOverlap = XMVectorInBounds(AxisdotOrigin, vExtents);
  NoIntersection = XMVectorOrInt(NoIntersection, XMVectorAndCInt(IsParallel, ParallelOverlap));

  if (!DirectX::Internal::XMVector3AnyTrue(NoIntersection))
  {
    // Store the x-component to *pDist
    XMStoreFloat(&Dist, t_min);
    return true;
  }

  Dist = 0.f;
  return false;
}
*/
