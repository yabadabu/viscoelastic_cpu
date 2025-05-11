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

VEC2 vogelDiskSample(int sampleIndex, int samplesCount, float phi)
{
  const float golden_angle = 2.4f;

  const float r = sqrtf((sampleIndex + 0.5f) / (float)samplesCount);
  const float theta = sampleIndex * golden_angle + phi;

  const float sine = sinf(theta);
  const float cosine = cosf(theta);
  return VEC2(r * cosine, r * sine);
}

void VogelHemiSphereSampler::initAxis( ) {
  aux1 = VEC3(1, 1, 1).cross(normal).normalized();
  aux2 = normal.cross(aux1).normalized();
};

VEC3 VogelHemiSphereSampler::get(int idx) const {
  VEC2 p = vogelDiskSample(idx, nsamples, phi * M_PI * 2.0f);
  VEC3 p3(p.x, 0, p.y);
  p3.y = sqrtf(1.0f - (p.x * p.x + p.y * p.y));
  p3 = p3.x * aux1 + p3.y * normal + p3.z * aux2;
  return p3;
}
