#pragma once

struct QUAT;
struct TRay;
struct MAT44;
struct VEC3;

#ifdef M_PI
#undef M_PI
#define M_PI 3.14159265358979323846f
#endif

#ifndef FLT_MAX
#define FLT_MAX          3.402823466e+38F        // max value
#endif

namespace Math {
  inline float fminf(float x, float y) { return (x < y) ? x : y; }
  inline float fmaxf(float x, float y) { return (y < x) ? x : y; }
  inline float clamp(float x, float vmin, float vmax) { return Math::fminf( vmax, fmaxf( x, vmin )); }
}

#if IN_PLATFORM_LINUX
#include <math.h>
#endif

// ---------------------------------------------------------------------
struct VEC2 {
  float x, y;
  VEC2() : x(0), y(0) {}
  VEC2(float ax, float ay) : x(ax), y(ay) {}
  float length() const { return sqrtf(x * x + y * y); }
  VEC2 normalized() const;
  float dot(const VEC2 b) const { return x * b.x + y * b.y; }
  void operator*=(float scale) {
    x *= scale;
    y *= scale;
  }
  void operator*=(VEC2 scale2) {
    x *= scale2.x;
    y *= scale2.y;
  }
  void operator+=(VEC2 delta) {
    x += delta.x;
    y += delta.y;
  }
  void operator-=(VEC2 delta) {
    x -= delta.x;
    y -= delta.y;
  }
  float lengthSquared() const { return (x * x + y * y); }
  VEC2 operator-() const { return VEC2(-x, -y); }
  float cross(const VEC2& b) const {
    return (y * b.x) - (x * b.y);
  }
  static VEC2 abs(VEC2 p) { return VEC2(fabsf(p.x), fabsf(p.y)); }
  static VEC2 Min(VEC2 a, VEC2 b);
  static VEC2 Max(VEC2 a, VEC2 b);
};
extern VEC2 operator-(const VEC2& a, const VEC2& b);
extern VEC2 operator+(const VEC2& a, const VEC2& b);
extern VEC2 operator*(const VEC2& a, const VEC2& b);
extern VEC2 operator*(const VEC2& a, float scalar);
extern bool operator==(const VEC3& v1, const VEC3& v2);
bool operator!=(const VEC2& v1, const VEC2& v2);

#include "vec3.h"

// ---------------------------------------------------------------------
struct VEC4 {
  float x, y, z, w;
  VEC4() : x(0), y(0), z(0), w(0) {}
  VEC4(float k) : x(k), y(k), z(k), w(k) {}
  VEC4(const VEC3& xyz, float new_w);
  constexpr VEC4(float ax, float ay, float az, float aw) : x(ax), y(ay), z(az), w(aw) {}
  VEC4 operator-() const { return VEC4(-x, -y, -z, -w); }
  float length() const { return sqrtf(x * x + y * y + z * z + w * w); }
  float dot(const VEC4 b) const { return x * b.x + y * b.y + z * b.z + w * b.w; }
  void operator*=(float scale) {
    x *= scale;
    y *= scale;
    z *= scale;
    w *= scale;
  }  
  void operator*=(VEC4 other) {
    x *= other.x;
    y *= other.y;
    z *= other.z;
    w *= other.w;
  }
  void operator+=(const VEC4 other) {
    x += other.x;
    y += other.y;
    z += other.z;
    w += other.w;
  }
  VEC3 xyz() const { return VEC3(x, y, z); }
};
extern VEC4 operator-(const VEC4& a, const VEC4& b);
extern VEC4 operator+(const VEC4& a, const VEC4& b);
extern VEC4 operator*(const VEC4& a, float scalar);
extern VEC4 operator*(float scalar, const VEC4& a);
extern VEC4 operator*(const VEC4& a, const VEC4& b);
bool operator!=(const VEC4& v1, const VEC4& v2);

// ---------------------------------------------------------------------
struct MAT44 {
  VEC4 x, y, z, w;
  MAT44() = default;
  MAT44(float sc);
  void loadIdentity();
  void loadTranslation(VEC3 new_trans);
  void loadPerspective(float fovy_radians, float aspect_ratio, float zmin, float zmax);
  MAT44 inverse() const;
  MAT44 transposed() const;
  void translation(VEC3 new_trans) {
    w.x = new_trans.x;
    w.y = new_trans.y;
    w.z = new_trans.z;
  }
  VEC3 translation( ) const {
    return VEC3( w.x, w.y, w.z );
  }
  float& operator()(int i, int j) {
    return *(&x.x + i * 4 + j);
  }
  const float& operator()(int i, int j) const {
    return *(&x.x + i * 4 + j);
  }
  VEC3 transformCoord(VEC3 p) const;
  VEC3 transformDir(VEC3 p) const;
  VEC3 getRow( int idx ) const { 
    const float *d = &x.x + idx * 4;
    return VEC3( d[0], d[1], d[2] );
  }

  static const MAT44 Identity;
  static MAT44 createLookAt(VEC3 src, VEC3 dst, VEC3 up);
  static MAT44 createPerspectiveFieldOfView(float fovy_radians, float aspect_ratio, float zmin, float zmax);
  static MAT44 createOrthographicOffCenter(float left, float right, float bottom, float top, float zNearPlane, float zFarPlane);
  static MAT44 createFromQuaternion(QUAT q);
  static MAT44 createScale(float sc);
  static MAT44 createScale(VEC3 sc);
  static MAT44 createTranslation(VEC3 delta);
  static MAT44 createFromAxisAngle(VEC3 axis, float angle);
  static MAT44 createLine(VEC3 source, VEC3 target);
};

extern MAT44 operator*(const MAT44& a, const MAT44& b);
extern VEC3 operator*(const VEC3& a, const MAT44& b);

// ---------------------------------------------------------------------
struct MAT33 {
  VEC3 x, y, z;
  float& operator()(int i, int j) {
    return *(&x.x + i * 3 + j);
  }
  const float& operator()(int i, int j) const {
    return *(&x.x + i * 3 + j);
  }
  MAT33 inverse() const;
  VEC3 transform(VEC3 p) const;
};

// ---------------------------------------------------------------------
struct QUAT {
  float x, y, z, w;
  QUAT() : x(0.f), y(0.f), z(0.f), w(1.f) { }
  QUAT(float ax, float ay, float az, float aw) : x(ax), y(ay), z(az), w(aw) { }
  MAT44 asMatrix() const;

  QUAT inverted() const { return QUAT(-x, -y, -z, w); }
  QUAT normalized() const {
    const float inv_factor = 1.f / (x * x + y * y + z * z + w * w);
    return QUAT(x * inv_factor, y * inv_factor, z * inv_factor, w * inv_factor);
  }
  VEC3 rotate(VEC3 v) const;
  float length() const { return sqrtf(x * x + y * y + z * z + w * w); }
  float dot(const QUAT b) const { return x * b.x + y * b.y + z * b.z + w * b.w; }

  static QUAT createFromAxisAngle(VEC3 axis, float angle);
  static QUAT createFromRotationMatrix(const MAT44& mtx);
  static QUAT createFromYawPitchRoll(float new_yaw, float new_pitch, float new_roll);
  static QUAT createFromDirectionToDirection(VEC3 src_dir, VEC3 dst_dir, float amount = 1.0f);
  static QUAT slerp(const QUAT& q1, const QUAT& q2, float t);
  static QUAT nlerp(const QUAT& q1, const QUAT& q2, float t);

};

extern QUAT operator*(const QUAT& q1, const QUAT& q2);
extern QUAT operator-(const QUAT& q1, const QUAT& q2);
extern QUAT operator+(const QUAT& q1, const QUAT& q2);
extern QUAT operator*(const QUAT& a, float scale);
extern bool operator!=(const QUAT& v1, const QUAT& v2);

// ---------------------------------------------------------------------
struct TRay {

  VEC3 position;
  VEC3 direction;

  TRay() : position(0, 0, 0), direction(0, 0, 1) {}
  TRay(const VEC3& pos, const VEC3& dir) : position(pos), direction(dir) {}
  VEC3 at(float t) const { return position + direction * t; }
  float intersectsSphere( VEC3 center, float r ) const;
  float intersectsPlane( VEC3 normal, float d ) const;
};

extern TRay operator*(const TRay& r, const MAT44& mtx);

// ---------------------------------------------------------------------
struct TViewport {
  float TopLeftX, TopLeftY;
  float Width, Height;
  float MinDepth, MaxDepth;
};

// ---------------------------------------------------------------------
#include "angular.h"
#include "transform.h"
#include "camera.h"
#include "aabb.h"

extern VEC3 closestPointToSegment(VEC3 src, VEC3 dst, VEC3 p);


