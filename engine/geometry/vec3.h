#pragma once

struct MAT44;

// ---------------------------------------------------------------------
struct ENGINE_API VEC3 {
  float x, y, z;
  VEC3() : x(0), y(0), z(0) {}
  VEC3(float ax, float ay, float az) : x(ax), y(ay), z(az) {}
  float length() const { return sqrtf(x * x + y * y + z * z); }
  float lengthSquared() const { return (x * x + y * y + z * z); }
  float dot(const VEC3 b) const { return x * b.x + y * b.y + z * b.z; }
  VEC3 cross(const VEC3 other) const;
  void normalize();
  VEC3 normalized() const;
  static void transform(const VEC3& src, const MAT44& mtx, VEC3& output);
  VEC3 operator-() const { return VEC3(-x, -y, -z); }
  VEC3 abs() const { return VEC3(fabsf(x), fabsf(y), fabsf(z)); }
  void operator*=(float scale) {
    x *= scale;
    y *= scale;
    z *= scale;
  }
  void operator+=(VEC3 delta) {
    x += delta.x;
    y += delta.y;
    z += delta.z;
  }
  void operator-=(VEC3 delta) {
    x -= delta.x;
    y -= delta.y;
    z -= delta.z;
  }
  void operator*=(VEC3 scale) {
    x *= scale.x;
    y *= scale.y;
    z *= scale.z;
  }
  float maxComponent() const;
  float minComponent() const;
  float operator[](int idx) const {
    assert(idx >= 0 && idx < 3);
    return *(&x + idx);
  }
  float& operator[](int idx) {
    assert(idx >= 0 && idx < 3);
    return *(&x + idx);
  }
  const char* c_str() const;
  static const VEC3 zero;
  static const VEC3 ones;
  static const VEC3 axis_x;
  static const VEC3 axis_y;
  static const VEC3 axis_z;
  static VEC3 Min(VEC3 a, VEC3 b);
  static VEC3 Max(VEC3 a, VEC3 b);
};
ENGINE_API extern VEC3 operator-(const VEC3& a, const VEC3& b);
ENGINE_API extern VEC3 operator+(const VEC3& a, const VEC3& b);
ENGINE_API extern VEC3 operator*(const VEC3& a, float scale);
ENGINE_API extern VEC3 operator*(float scale, const VEC3& a);
ENGINE_API extern VEC3 operator*(const VEC3 a, const VEC3 b);
ENGINE_API extern VEC3 operator/(float numerator, const VEC3& a);
ENGINE_API bool operator!=(const VEC3& v1, const VEC3& v2);

