#ifndef INC_ANGULAR_H_
#define INC_ANGULAR_H_

// Angular
VEC3  getVectorFromYaw(float yaw);
float getYawFromVector(const VEC3& v);
VEC3  getVectorFromYawAndPitch(float yaw, float pitch);
void  getYawPitchFromVector(const VEC3& v, float* yaw, float* pitch);

VEC3  getRightXZOf(const VEC3& v);
VEC3  getLeftXZOf(const VEC3& v);
float wrapAngle(float rad);

#define deg2rad( degrees ) ( (degrees) * (float)M_PI / 180.f )
#define rad2deg( radians ) ( (radians) * 180.f / (float)M_PI )

VEC2 vogelDiskSample(int sampleIndex, int samplesCount, float phi);

struct VogelHemiSphereSampler {
  VEC3  normal = VEC3(0,1,0);
  float phi = 0.0f;
  int   nsamples = 16;
  VEC3  aux1, aux2;
  void initAxis();
  VEC3 get(int idx) const;
};

#endif
