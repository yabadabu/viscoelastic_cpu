#include "platform.h"

// ----------------- Angular 
VEC3 getVectorFromYaw(float yaw) {
  return VEC3(sinf(yaw)
    , 0
    , cosf(yaw));
}

float getYawFromVector(const VEC3& v) {
  return atan2f(v.x, v.z);
}

VEC3 getVectorFromYawAndPitch(float yaw, float pitch) {
  return VEC3(sinf(yaw) * cosf(pitch)
    , sinf(pitch)
    , cosf(yaw) * cosf(pitch)
  );
}

void getYawPitchFromVector(const VEC3& v, float* yaw, float* pitch) {
  assert(yaw && pitch);
  *yaw = atan2f(v.x, v.z);
  float mdo_xz = sqrtf(v.x * v.x + v.z * v.z);
  *pitch = atan2f(v.y, mdo_xz);
}

VEC3 getRightXZOf(const VEC3& v) {
  return VEC3(-v.z, v.y, v.x);
}
VEC3 getLeftXZOf(const VEC3& v) {
  return VEC3(v.z, v.y, -v.x);
}

float wrapAngle(float rad) {
  const float pi = (float)M_PI;
  if (rad < -pi)
    rad += 2 * pi;
  else if (rad > pi)
    rad -= 2 * pi;
  return rad;
}
