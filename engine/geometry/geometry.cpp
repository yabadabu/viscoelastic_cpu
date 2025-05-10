#include "platform.h"
#include "geometry.h"
#include "angular.h"

VEC4::VEC4(const VEC3& xyz, float new_w)
  : x(xyz.x)
  , y(xyz.y)
  , z(xyz.z)
  , w(new_w)
{}

// ---------------------------------------------------------------
VEC2 operator-(const VEC2& a, const VEC2& b) {
  return VEC2(a.x - b.x, a.y - b.y);
}

VEC2 operator+(const VEC2& a, const VEC2& b) {
  return VEC2(a.x + b.x, a.y + b.y);
}

VEC2 operator*(const VEC2& a, const VEC2& b) {
  return VEC2(a.x * b.x, a.y * b.y);
}

VEC2 operator*(const VEC2& a, float scalar) {
  return VEC2(a.x * scalar, a.y * scalar);
}

VEC2 VEC2::Min(VEC2 a, VEC2 b) {
  return VEC2(Math::fminf(a.x, b.x), Math::fminf(a.y, b.y));
}

VEC2 VEC2::Max(VEC2 a, VEC2 b) {
  return VEC2(Math::fmaxf(a.x, b.x), Math::fmaxf(a.y, b.y));
}

VEC2 VEC2::normalized() const {
	float inv_len = length();
	if (inv_len != 0) {
		inv_len = 1.0f / inv_len;
		return VEC2(x * inv_len, y * inv_len);
	}
	return *this;
}

bool operator!=(const VEC2& v1, const VEC2& v2)
{
  return v1.x != v2.x || v1.y != v2.y;
}


// ---------------------------------------------------------------
VEC4 operator-(const VEC4& a, const VEC4& b) {
  return VEC4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

VEC4 operator+(const VEC4& a, const VEC4& b) {
  return VEC4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

VEC4 operator*(const VEC4& a, float scalar) {
  return VEC4(a.x * scalar, a.y * scalar, a.z * scalar, a.w * scalar);
}

VEC4 operator*(const VEC4& a, const VEC4& b) {
  return VEC4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);
}

VEC4 operator*(float scalar, const VEC4& a) {
  return VEC4(a.x * scalar, a.y * scalar, a.z * scalar, a.w * scalar);
}

bool operator!=(const VEC4& v1, const VEC4& v2)
{
  return v1.x != v2.x || v1.y != v2.y || v1.z != v2.z || v1.w != v2.w;
}

// ---------------------------------------------------------------
const VEC3 VEC3::zero(0, 0, 0);
const VEC3 VEC3::ones(1, 1, 1);
const VEC3 VEC3::axis_x(1, 0, 0);
const VEC3 VEC3::axis_y(0, 1, 0);
const VEC3 VEC3::axis_z(0, 0, 1);

VEC3 operator-(const VEC3& a, const VEC3& b) {
  return VEC3(a.x - b.x, a.y - b.y, a.z - b.z);
}

VEC3 operator+(const VEC3& a, const VEC3& b) {
  return VEC3(a.x + b.x, a.y + b.y, a.z + b.z);
}

VEC3 operator*(const VEC3& a, float scale) {
  return VEC3(a.x * scale, a.y * scale, a.z * scale);
}

VEC3 operator*(float scale, const VEC3& a) {
  return VEC3(a.x * scale, a.y * scale, a.z * scale);
}

VEC3 operator*(const VEC3 a, const VEC3 b) {
  return VEC3(a.x * b.x, a.y * b.y, a.z * b.z);
}

VEC3 operator*(const VEC3& a, const MAT44& b) {
  return b.transformCoord(a);
}

VEC3 operator/(float numerator, const VEC3& a) {
  return VEC3(numerator / a.x, numerator / a.y, numerator / a.z);
}

float VEC3::maxComponent() const {
  return Math::fmaxf(x, Math::fmaxf(y, z));
}

float VEC3::minComponent() const {
  return Math::fminf(x, Math::fminf(y, z));
}

// Required by some meta
bool operator==(const VEC3& v1, const VEC3& v2) {
  return v1.x == v2.x && v1.y == v2.y && v1.z == v2.z;
}

bool operator!=(const VEC3& v1, const VEC3& v2) {
  return !(v1 == v2);
}

VEC3 VEC3::cross(const VEC3 b) const {
  return VEC3((y * b.z) - (z * b.y)
    , (z * b.x) - (x * b.z)
    , (x * b.y) - (y * b.x)
  );
}

void VEC3::normalize() {
  float inv_len = length();
  if (inv_len != 0) {
    inv_len = 1.0f / inv_len;
    x *= inv_len;
    y *= inv_len;
    z *= inv_len;
  }
}

VEC3 VEC3::normalized() const {
  float inv_len = length();
  if (inv_len != 0) {
    inv_len = 1.0f / inv_len;
    return VEC3(x * inv_len, y * inv_len, z * inv_len);
  }
  return *this;
}

VEC3 VEC3::Min(VEC3 a, VEC3 b) {
  return VEC3(Math::fminf(a.x, b.x), Math::fminf(a.y, b.y), Math::fminf(a.z, b.z));
}

VEC3 VEC3::Max(VEC3 a, VEC3 b) {
  return VEC3(Math::fmaxf(a.x, b.x), Math::fmaxf(a.y, b.y), Math::fmaxf(a.z, b.z));
}

void VEC3::transform(const VEC3& src, const MAT44& mtx, VEC3& output) {
  output = mtx.transformCoord(src);
}


// ---------------------------------------------------------------
const MAT44 MAT44::Identity(1.0f);

MAT44::MAT44(float sc) {
  loadIdentity();
  x.x = y.y = z.z = sc;
}

void MAT44::loadIdentity() {
  x = VEC4(1, 0, 0, 0);
  y = VEC4(0, 1, 0, 0);
  z = VEC4(0, 0, 1, 0);
  w = VEC4(0, 0, 0, 1);
}

MAT44 MAT44::createFromAxisAngle(VEC3 axis, float angle) {
  return QUAT::createFromAxisAngle(axis, angle).asMatrix();
}

MAT44 MAT44::createLine(VEC3 source, VEC3 target) {
  MAT44 m = MAT44::Identity;
  m.z = VEC4(target - source, 0.f);
  m.w = VEC4(source, 1.f);
  return m;
}

MAT44 MAT44::createLookAt(VEC3 src, VEC3 dst, VEC3 up_axis) {
  // Changing the sign of front!
  VEC3 front = src - dst;
  front.normalize();

  VEC3 left = up_axis.cross(front);
  left.normalize();
  VEC3 up = front.cross(left);

  // Build view matrix
  MAT44 m;
  m.x.x = left.x;
  m.y.x = left.y;
  m.z.x = left.z;
  m.x.y = up.x;
  m.y.y = up.y;
  m.z.y = up.z;
  m.x.z = front.x;
  m.y.z = front.y;
  m.z.z = front.z;
  m.x.w = m.y.w = m.z.w = 0.0f;

  m.w.x = -left.dot(src);
  m.w.y = -up.dot(src);
  m.w.z = -front.dot(src);
  m.w.w = 1.0f;
  return m;
}

MAT44 MAT44::createScale(float sc) {
  return MAT44(sc);
}

MAT44 MAT44::createScale(VEC3 sc) {
  MAT44 m = MAT44::Identity;
  m.x.x = sc.x;
  m.y.y = sc.y;
  m.z.z = sc.z;
  return m;
}

MAT44 MAT44::createTranslation(VEC3 delta) {
  MAT44 m = MAT44::Identity;
  m.w.x = delta.x;
  m.w.y = delta.y;
  m.w.z = delta.z;
  return m;
}

MAT44 MAT44::createFromQuaternion(QUAT q) {
  return q.asMatrix();
}


// ---------------------------------------------------------------------------------
MAT44 MAT44::createPerspectiveFieldOfView(float fovy_radians, float aspect_ratio, float zmin, float zmax) {
  MAT44 m;
  m.loadPerspective(fovy_radians, aspect_ratio, zmin, zmax);
  return m;
}

void MAT44::loadPerspective(float fovy_radians, float aspect, float znear, float zfar) {
  float angle = fovy_radians * 0.5f;
  assert(sinf(angle) != 0.0f);
  float cotangent = cosf(angle) / sinf(angle);
  if (zfar < znear)
    zfar = znear + 0.1f;
  float delta_z = znear - zfar;

  loadIdentity();
  assert(aspect != 0.0f && delta_z != 0.0f);
  x.x = cotangent / aspect;
  y.y = cotangent;
  z.z = (zfar + znear) / delta_z; // -(far + near) / delta_z
  w.z = 2.0f * (zfar * znear) / delta_z; // -2 * (far * near) / delta_z
  z.w = -1.0f;
  w.w = 0.0f;
}

// ---------------------------------------------------------------------------------
MAT44 MAT44::createOrthographicOffCenter(float x_min, float x_max, float y_min, float y_max, float z_min, float z_max) {
  MAT44 m;
  m.loadIdentity();

  // (RH) l=x_min; r=x_max; b=y_min; t=y_max; zn=z_min; zf=z_max;
  //
  // 2/(r-l)      0            0           0
  // 0            2/(t-b)      0           0
  // 0            0            1/(zn-zf)   0
  // (l+r)/(l-r)  (t+b)/(b-t)  zn/(zn-zf)  1

  assert(x_max != x_min);
  assert(y_max != y_min);
  assert(z_max != z_min);

  m.x.x = 2.0f / (x_max - x_min);
  m.y.y = 2.0f / (y_max - y_min);
  m.z.z = 1.0f / (z_min - z_max);
  m.w.x = (x_min + x_max) / (x_min - x_max);
  m.w.y = (y_max + y_min) / (y_min - y_max);
  m.w.z = z_min / (z_min - z_max);
  return m;
}

// ---------------------------------------------------------------------------------
MAT44 operator*(const MAT44& a, const MAT44& b) {
  MAT44 t;
  const float* ma = (const float*)&b.x.x;
  const float* mb = (const float*)&a.x.x;
  float* mr = (float*)&t;
  for (unsigned int i = 0; i < 4; ++i) {
    const float* maa = ma;
    const float* mbb = mb + (i << 2);
    float* mrr = mr + (i << 2);
    for (unsigned int j = 0; j < 4; ++j, ++maa) {
      *mrr++ = maa[0] * mbb[0]
        + maa[4] * mbb[1]
        + maa[8] * mbb[2]
        + maa[12] * mbb[3]
        ;
    }
  }
  MAT44 r;
  memcpy(&r, &t, sizeof(MAT44));
  return r;
}

MAT44 MAT44::inverse() const {

  MAT44 inverse;
  inverse.loadIdentity();

  MAT44 temp = (*this);

  unsigned int m = 4, n = 4;
  for (unsigned i = 0; i < m; i++) {
    // Look for largest element in column
    unsigned swap_idx = i;
    for (unsigned j = i + 1; j < m; j++) {	// m or n
      if (fabs(temp(j, i)) > fabs(temp(swap_idx, i)))
        swap_idx = j;
    }
    if (swap_idx != i) {
      // Swap rows.
      for (unsigned k = 0; k < n; k++) {
        float t = temp(i, k); temp(i, k) = temp(swap_idx, k); temp(swap_idx, k) = t;
        t = inverse(i, k); inverse(i, k) = inverse(swap_idx, k); inverse(swap_idx, k) = t;
      }
    }

    assert(fabsf(temp(i, i)) >= 1e-10);
    float t = 1.0f / temp(i, i);

    for (unsigned k = 0; k < n; k++) {//m or n
      temp(i, k) *= t;
      inverse(i, k) *= t;
    }

    for (unsigned j = 0; j < m; j++) { // m or n
      if (j != i) {
        float nt = temp(j, i);
        for (unsigned k = 0; k < n; k++) { //m or n
          temp(j, k) -= (temp(i, k) * nt);
          inverse(j, k) -= (inverse(i, k) * nt);
        }
      }
    }
  }
  return inverse;
}


void MAT44::loadTranslation(VEC3 new_trans) {
  loadIdentity();
  w.x = new_trans.x;
  w.y = new_trans.y;
  w.z = new_trans.z;
}


VEC3 MAT44::transformCoord(VEC3 a) const {
  float rx = a.x * x.x + a.y * y.x + a.z * z.x + w.x;
  float ry = a.x * x.y + a.y * y.y + a.z * z.y + w.y;
  float rz = a.x * x.z + a.y * y.z + a.z * z.z + w.z;
  float rw = a.x * x.w + a.y * y.w + a.z * z.w + w.w;
  if (rw != 0.0f) {
    float inv_w = 1.0f / rw;
    rx *= inv_w;
    ry *= inv_w;
    rz *= inv_w;
  }
  return VEC3(rx, ry, rz);
}


VEC3 MAT44::transformDir(VEC3 a) const {
  float rx = a.x * x.x + a.y * y.x + a.z * z.x;
  float ry = a.x * x.y + a.y * y.y + a.z * z.y;
  float rz = a.x * x.z + a.y * y.z + a.z * z.z;
  float rw = a.x * x.w + a.y * y.w + a.z * z.w;
  if (rw != 0.0f) {
    float inv_w = 1.0f / rw;
    rx *= inv_w;
    ry *= inv_w;
    rz *= inv_w;
  }
  return VEC3(rx, ry, rz);
}


MAT44 MAT44::transposed() const {

  MAT44 temp = (*this);

  for (int i = 0; i < 4; i++) {
    for (int j = i + 1; j < 4; j++) {
      float save = temp(i, j);
      temp(i, j) = temp(j, i);
      temp(j, i) = save;
    }
  }

  return temp;
}

MAT33 MAT33::inverse() const {
  // computes the inverse of a matrix m
  const MAT33& m = *this;
  float det = m(0, 0) * (m(1, 1) * m(2, 2) - m(2, 1) * m(1, 2)) -
    m(0, 1) * (m(1, 0) * m(2, 2) - m(1, 2) * m(2, 0)) +
    m(0, 2) * (m(1, 0) * m(2, 1) - m(1, 1) * m(2, 0));

  float invdet = 1.0f / det;

  MAT33 minv; // inverse of matrix m
  minv(0, 0) = (m(1, 1) * m(2, 2) - m(2, 1) * m(1, 2)) * invdet;
  minv(0, 1) = (m(0, 2) * m(2, 1) - m(0, 1) * m(2, 2)) * invdet;
  minv(0, 2) = (m(0, 1) * m(1, 2) - m(0, 2) * m(1, 1)) * invdet;
  minv(1, 0) = (m(1, 2) * m(2, 0) - m(1, 0) * m(2, 2)) * invdet;
  minv(1, 1) = (m(0, 0) * m(2, 2) - m(0, 2) * m(2, 0)) * invdet;
  minv(1, 2) = (m(1, 0) * m(0, 2) - m(0, 0) * m(1, 2)) * invdet;
  minv(2, 0) = (m(1, 0) * m(2, 1) - m(2, 0) * m(1, 1)) * invdet;
  minv(2, 1) = (m(2, 0) * m(0, 1) - m(0, 0) * m(2, 1)) * invdet;
  minv(2, 2) = (m(0, 0) * m(1, 1) - m(1, 0) * m(0, 1)) * invdet;
  return minv;
}

VEC3 MAT33::transform(const VEC3 a) const {
  return VEC3(
    a.x * x.x + a.y * y.x + a.z * z.x,
    a.x * x.y + a.y * y.y + a.z * z.y,
    a.x * x.z + a.y * y.z + a.z * z.z
  );
}


// ---------------------------------------------------------------
QUAT QUAT::nlerp(const QUAT& q1, const QUAT& q2, float t) {
  return (q1 * (1.f - t) + q2 * t).normalized();
}

// ---------------------------------------------------------------
QUAT QUAT::createFromAxisAngle(VEC3 axis, float angle) {
  float s = sinf(angle * 0.5f);
  return QUAT(
    axis.x * s,
    axis.y * s,
    axis.z * s,
    cosf(angle * 0.5f)
  );
}

// ---------------------------------------------------------------
QUAT QUAT::createFromDirectionToDirection(VEC3 src_dir, VEC3 dst_dir, float amount) {
  VEC3 perp_axis = src_dir.cross(dst_dir);
  float mdo = perp_axis.length();
  if (mdo < 1e-5f) {
    if( src_dir.dot(dst_dir) > (1.0f - 1e-5f))
      return QUAT();
    VEC3 aux_axis = VEC3(src_dir.y, src_dir.z, -src_dir.x);
    perp_axis = aux_axis.cross(src_dir);
    perp_axis.normalize();
    return QUAT(perp_axis.x, perp_axis.y, perp_axis.z, 0);
  }
  perp_axis *= (1.0f / mdo);
  float cos_angle = src_dir.dot(dst_dir);
  cos_angle = Math::fminf(1.0f, cos_angle);
  cos_angle = Math::fmaxf(-1.0f, cos_angle);
  float angle = acosf(cos_angle);
  angle *= amount;
  return QUAT::createFromAxisAngle(perp_axis, angle);
}

MAT44 QUAT::asMatrix() const {
  MAT44 pm;
  pm.x.x = 1.0f - 2.0f * y * y - 2.0f * z * z;
  pm.x.y = 2.0f * x * y + 2.0f * w * z;
  pm.x.z = 2.0f * z * x - 2.0f * w * y;
  pm.x.w = 0.0f;
  pm.y.x = 2.0f * x * y - 2.0f * w * z;
  pm.y.y = 1.0f - 2.0f * x * x - 2.0f * z * z;
  pm.y.z = 2.0f * y * z + 2.0f * w * x;
  pm.y.w = 0.0f;
  pm.z.x = 2.0f * z * x + 2.0f * w * y;
  pm.z.y = 2.0f * y * z - 2.0f * w * x;
  pm.z.z = 1.0f - 2.0f * x * x - 2.0f * y * y;
  pm.z.w = 0.0f;
  pm.x.w = 0.0f;
  pm.y.w = 0.0f;
  pm.z.w = 0.0f;
  pm.w.w = 1.0f;
  return pm;
}

QUAT operator*(const QUAT& q2, const QUAT& q1) {
  return QUAT(q1.w * q2.x + q2.w * q1.x + q1.y * q2.z - q1.z * q2.y
    , q1.w * q2.y + q2.w * q1.y + q1.z * q2.x - q1.x * q2.z
    , q1.w * q2.z + q2.w * q1.z + q1.x * q2.y - q1.y * q2.x
    , q1.w * q2.w - q1.x * q2.x - q1.y * q2.y - q1.z * q2.z
  );
}

bool operator!=(const QUAT& v1, const QUAT& v2)
{
  return v1.x != v2.x || v1.y != v2.y || v1.z != v2.z || v1.w != v2.w;
}


// https://blog.molecular-matters.com/2013/05/24/a-faster-quaternion-vector-multiplication/
VEC3 QUAT::rotate(VEC3 v) const {
  VEC3 r(x, y, z);
  VEC3 t = 2 * r.cross(v);
  return  v + w * t + r.cross(t);

  //// From http://gamedev.stackexchange.com/questions/28395/rotating-vector3-by-a-quaternion
  //// Extract the vector part of the quaternion
  //VEC3 u(x, y, z);

  //// Extract the scalar part of the quaternion
  //float s = w;

  //// Do the math
  //return 2.0f * u.dot(v) * u
  //  + (s*s - u.dot(u)) * v
  //  + 2.0f * s * u.Cross(v);
}

QUAT QUAT::createFromRotationMatrix(const MAT44& m) {

  float r22 = m.z.z;
  if (r22 <= 0.f) {
    float dif10 = m.y.y - m.x.x;
    float omr22 = 1.f - r22;
    if (dif10 <= 0.f) {
      float fourXSqr = omr22 - dif10;
      float inv4x = 0.5f / sqrtf(fourXSqr);
      return QUAT(
        fourXSqr * inv4x,
        (m.x.y + m.y.x) * inv4x,
        (m.x.z + m.z.x) * inv4x,
        (m.y.z - m.z.y) * inv4x
      );
    }
    else {
      float fourYSqr = omr22 + dif10;
      float inv4y = 0.5f / sqrtf(fourYSqr);
      return QUAT(
        (m.x.y + m.y.x) * inv4y,
        fourYSqr * inv4y,
        (m.y.z + m.z.y) * inv4y,
        (m.z.x - m.x.z) * inv4y
      );
    }
  }
  else
  {
    float sum10 = m.y.y + m.x.x;
    float opr22 = 1.f + r22;
    if (sum10 <= 0.f) {
      float fourZSqr = opr22 - sum10;
      float inv4z = 0.5f / sqrtf(fourZSqr);
      return QUAT(
        (m.x.z + m.z.x) * inv4z,
        (m.y.z + m.z.y) * inv4z,
        fourZSqr * inv4z,
        (m.x.y - m.y.x) * inv4z
      );
    }
    else {
      float fourWSqr = opr22 + sum10;
      float inv4w = 0.5f / sqrtf(fourWSqr);
      return QUAT(
        (m.y.z - m.z.y) * inv4w,
        (m.z.x - m.x.z) * inv4w,
        (m.x.y - m.y.x) * inv4w,
        fourWSqr * inv4w
      );
    }
  }

  /*
  QUAT q;
  float trace = m.x.x + m.y.y + m.z.z;
  if ( trace > 0 ) {
    // |w| > 1/2, may as well choose w > 1/2
    float fRoot = sqrtf(trace + 1.0f);  // 2w
    q.w = 0.5f*fRoot;
    fRoot = 0.5f/fRoot;  // 1/(4w)
    q.x = (m.y.z-m.z.y)*fRoot;
    q.y = (m.z.x-m.x.z)*fRoot;
    q.z = (m.x.y-m.y.x)*fRoot;

  } else {
    //    assert( !"This code is wrong... Should negate it?\n");
    // |w| <= 1/2
    int i = 0;
    if ( m.y.y > m.x.x )
      i = 1;
    if ( m.z.z > m(i,i) )
      i = 2;
    int j = (i+1) % 3;
    int k = (j+1) % 3;

    float fRoot = std::sqrtf( m(i,i)- m(j,j)- m(k,k) + 1.0f);
    float* apfQuat[3] = { &q.x, &q.y, &q.z };
    *apfQuat[i] = -0.5f*fRoot;
    fRoot = 0.5f/fRoot;
    q.w = (m(k,j)-m(j,k))*fRoot;
    *apfQuat[j] = -(m(j,i)+m(i,j))*fRoot;
    *apfQuat[k] = -(m(k,i)+m(i,k))*fRoot;
  }
  return q;
   */
}

QUAT QUAT::createFromYawPitchRoll(float yaw, float pitch, float roll) {
  return
    QUAT::createFromAxisAngle(VEC3(0, 0, 1), roll)
    *
    QUAT::createFromAxisAngle(VEC3(1, 0, 0), pitch)
    *
    QUAT::createFromAxisAngle(VEC3(0, 1, 0), yaw)
    ;
}

QUAT operator-(const QUAT& q1, const QUAT& q2) {
  return QUAT(q1.x - q2.x, q1.y - q2.y, q1.z - q2.z, q1.w - q2.w);
}

QUAT operator+(const QUAT& q1, const QUAT& q2) {
  return QUAT(q1.x + q2.x, q1.y + q2.y, q1.z + q2.z, q1.w + q2.w);
}

QUAT operator*(const QUAT& q1, float f) {
  return QUAT(q1.x * f, q1.y * f, q1.z * f, q1.w * f);
}

// -------------------------------------
TRay operator*(const TRay& r, const MAT44& mtx) {
  return TRay(
    mtx.transformCoord(r.position)
    , mtx.transformDir(r.direction)
  );
}

// -------------------------------------
#if ENABLE_GEOMETRY_UNIT_TESTS
#include "transform.h"
void geometryUnitTests() {

  {
    QUAT q = QUAT::createFromAxisAngle(VEC3(0, 1, 0), deg2rad(45.f));
    QUAT qerr = q - QUAT(0, 0.382683f, 0, 0.923880f);
    assert(qerr.length() < 1e-3f);
    MAT44 m = MAT44::createFromQuaternion(q);
    VEC4 m0 = m.x - VEC4(0.707107f, 0, -0.707107f, 0.f); assert(m0.length() < 1e-3f);
    VEC4 m1 = m.y - VEC4(0.f, 1.f, 0.f, 0.f); assert(m1.length() < 1e-3f);
    VEC4 m2 = m.z - VEC4(0.707107f, 0, 0.707107f, 0.f); assert(m2.length() < 1e-3f);
    VEC4 m3 = m.w - VEC4(0.f, 0.f, 0.f, 1.f); assert(m3.length() < 1e-3f);
    QUAT q2 = QUAT::createFromRotationMatrix(m);
    QUAT qerr2 = q2 - q;
    assert(qerr2.length() < 1e-3f);

    QUAT q3 = QUAT::createFromAxisAngle(VEC3(1, 0, 0), deg2rad(30.f));
    QUAT q4 = q * q3;
    QUAT qerr4 = q4 - QUAT(0.239118f, 0.369644f, 0.099046f, 0.892399f);
    assert(qerr4.length() < 1e-3f);

  }
  {
    VEC3 up(0, -1, 0.5f); up.normalize();
    MAT44 m = MAT44::createLookAt(VEC3(-1, 2, -1), VEC3(-1, 1.5f, -2), up);
    VEC4 m0 = m.x - VEC4(-1, 0.f, 0.f, 0.f); assert(m0.length() < 1e-3f);
    VEC4 m1 = m.y - VEC4(0.f, -0.894427f, 0.447214f, 0.f); assert(m1.length() < 1e-3f);
    VEC4 m2 = m.z - VEC4(0.f, 0.447214f, 0.894427f, 0.f); assert(m2.length() < 1e-3f);
    VEC4 m3 = m.w - VEC4(-1.f, 2.236068f, 0.f, 1.f); assert(m3.length() < 1e-3f);
    QUAT q = QUAT::createFromRotationMatrix(m);
    QUAT qerr2 = q - QUAT(0.f, 0.229753f, 0.973249f, 0.f);
    assert(qerr2.length() < 1e-3f);
  }
  {
    QUAT q = QUAT::createFromAxisAngle(VEC3(0, 1, 0), deg2rad(45.f));
    assert(q.x == 0.f);
    assert(fabsf(q.y - 0.382683426f) < 1e-3f);
    assert(q.z == 0.f);
    assert(fabsf(q.w - 0.923879504f) < 1e-3f);
    VEC3 p0(1, 0, 0);
    VEC3 p1 = q.rotate(p0);
    assert((p1 - VEC3(0.707f, 0.f, -0.707f)).length() < 1e-3f);
  }
  {
    VEC3 p0(1, 0, 0);
    VEC3 p1 = VEC3(0.707f, 0.f, -0.707f);
    QUAT q = QUAT::createFromDirectionToDirection(p0, p1);
    VEC3 p2 = q.rotate(p0);
    assert((p2 - p1).length() < 1e-3f);
  }
  {
    TTransform t1(VEC3(1, 0, 1), QUAT::createFromAxisAngle(VEC3(0, 1, 0), deg2rad(60)), VEC3::ones);
    TTransform t2(VEC3(2, 0, -1), QUAT::createFromAxisAngle(VEC3(1, 0, 0), deg2rad(-30)), VEC3::ones);
    TTransform t3 = t1.combineWith(t2);
    assert((t3.getPosition() - VEC3(1.13397455f, 0.f, -1.23205090f)).length() < 1e-3f);
    assert((QUAT(t3.rotation) - QUAT(-0.224143863f, 0.4829629f, 0.1294095f, 0.8365162f)).length() < 1e-3f);
  }
}
#endif

QUAT QUAT::slerp(const QUAT& q1, const QUAT& q2, float t) {
  assert( t >= 0 && t <= 1.0f);

  float n3 = q1.dot(q2);

  if (fabsf(n3) >= 1.f)
    return q1;

  bool flag = false;
  if (n3 < 0) {
    flag = true;
    n3 = -n3;
  }

  float n4 = acosf(n3);
  float n2 = sinf((1.0f - t) * n4);
  float n1 = flag ? -sinf(t * n4) : sinf(t * n4);
  float n5 = 1.0f / sinf(n4);

  return ( q1 * ( n2 * n5 ) + q2 * ( n1 * n5));
}

// ---------------------------------------------------------------------
VEC3 closestPointToSegment(VEC3 src, VEC3 dst, VEC3 p) {
  VEC3 seg = dst - src;
  VEC3 pt = p - src;
  float seg_length = seg.length();
  assert(seg_length > 0);
  if (seg_length <= 0)
    return src;
  VEC3 norm_seg = seg.normalized();
  float proj = pt.dot(norm_seg);
  VEC3 closest;
  if (proj <= 0) {
    return src;
  } else if (proj >= seg_length) {
    return dst;
  }
  VEC3 proj_v = norm_seg * proj;
  return proj_v + src;
}

float TRay::intersectsSphere( VEC3 center, float radius ) const {

  // Vector from the ray's origin to the sphere's center
  VEC3 oc = position - center;

  // Calculate coefficients for the quadratic equation
  float a = direction.dot( direction );
  float b = oc.dot(direction);
  float c = oc.dot( oc ) - radius * radius;

  // Calculate the discriminant
  float discriminant = b * b - a * c;

  // If the discriminant is negative, there is no intersection
  if (discriminant < 0.0f)
      return -1.0f;
  float root = sqrtf( discriminant );

  // Calculate the two possible solutions for t
  float t1 = (-b - root) / a;
  float t2 = (-b + root) / a;

  // Check if the intersections are in front of the ray
  if (t1 >= 0 && t2 >= 0) {
      // Return the nearest intersection point
      return Math::fminf(t1, t2);
  } else if (t1 >= 0) {
      return t1;
  } else if (t2 >= 0) {
      return t2;
  }

  return -1.0f; // No intersection
}

float TRay::intersectsPlane( VEC3 normal, float d ) const {
  float num = normal.dot( position ) - d;
  float den = normal.dot( direction );
  return -num / den;
}


/*

// --------------------TMatrix operator*--------------------------
// ---------------------------------------------------------------
TMatrix operator*( const TMatrix &a, const TMatrix &b ) {
  assert( isAligned16( &a ) );
  assert( isAligned16( &b ) );

  //vfpu_generic_matrix_multiply( &r, &a, &b );
  sceVfpuMatrix4Mul ( &r, &b, &a );
  return r;
}
#else
TMatrix operator*( const TMatrix &a, const TMatrix &b ) {
  TMatrix t;
  const float* ma = (const float*)&b.x.x;
  const float* mb = (const float*)&a.x.x;
  float* mr = (float*)&t;
  for (unsigned int i = 0; i < 4; ++i) {
    const float *maa = ma;
    const float *mbb = mb + (i<<2);
          float *mrr = mr + (i<<2);
    for (unsigned int j = 0; j < 4; ++j, ++maa) {
      *mrr++ = maa[  0 ] * mbb[ 0 ]
             + maa[  4 ] * mbb[ 1 ]
             + maa[  8 ] * mbb[ 2 ]
             + maa[ 12 ] * mbb[ 3 ]
             ;
    }
  }
  TMatrix r;
  memcpy(&r,&t,sizeof(TMatrix));
  return r;
}
#endif



// ----------------------------------------------------------
 bool RayTriangleIntersect(const TPoint &origin, const TPoint &direction,
               const TPoint &vert0, const TPoint &vert1, const TPoint &vert2,
               float &u, float &v, float &distance)
 {
  distance = 0;

    TPoint edge1 = vert1 - vert0;
    TPoint edge2 = vert2 - vert0;

    TPoint tvec, pvec, qvec;
    float det, inv_det;

    pvec = CrossProduct(direction, edge2);

    det = dotProduct(edge1, pvec);



  // No Culling
    if (det > -0.00001f)
        return false; // No colisiona

    inv_det = 1.0f / det;

    tvec = origin - vert0;

    u = dotProduct(tvec, pvec) * inv_det;
    if (u < -0.001f || u > 1.001f)
        return false;

    qvec = CrossProduct(tvec, edge1);

    v = dotProduct(direction, qvec) * inv_det;
    if (v < -0.001f || u + v > 1.001f)
        return false;

    distance = dotProduct(edge2, qvec) * inv_det;

    if (distance <= 0)
        return false;


    return true;
 }


// ---------------------------------------------------------------------
bool segmentsIntersect( const TPoint &src, const TPoint &dst,
                       const TPoint &corner, const TPoint &next_corner, TPoint *intersect ) {


  float d = ( next_corner.z - corner.z ) * ( dst.x - src.x ) -
            ( next_corner.x - corner.x ) * ( dst.z - src.z ) ;

  if ( d == 0.0f ) return false ; // Son paralelas

  float na = ( next_corner.x - corner.x ) * ( src.z - corner.z ) -
             ( next_corner.z - corner.z ) * ( src.x - corner.x ) ;

  float nb = ( dst.x - src.x ) * ( src.z - corner.z ) -
             ( dst.z - src.z ) * ( src.x - corner.x ) ;

  float ua = na / d;
  float ub = nb / d;

  if(ua >= 0.0f && ua <= 1.0f && ub >= 0.0f && ub <= 1.0f)
  {
    if ( intersect ) {
      intersect->x = src.x + ua * ( dst.x - src.x );
      intersect->y = 0.0f;
      intersect->z = src.z + ua * ( dst.z - src.z );
    }
    return true;
  }

  return false;
}

bool rayIntersectXZ( const TPoint &origin_a, const TPoint &direction, TPoint *intersection  = NULL)
{
  // Mirar si choca con el suelo
  float Vd = direction.y;
  if(Vd < 0) // No es paralelo o mirando en dirección contraria al plano
  {
    float V0 = -origin_a.y;
    float t = V0 / Vd;
    //if( t > 0) // Rayo por encima del plano // Me da igual que esté debajo del plano? Entonces tira patrás
    //{
      if( intersection )
      *intersection =  origin_a + direction * t;
      return true;
    //}
    //return false;
  }
  return false;
}

bool rayIntersectPlane( const TPoint &segment_origin, const TPoint &segment_dir, const TPoint &plane_point, const TPoint &plane_normal, TPoint *intersection ) {
  TPoint n0 = (plane_point - segment_origin);
  float n = n0.dot( plane_normal );
  float den = segment_dir.dot( plane_normal );
  if( den == 0.0f ) {
    if( n == 0.0f)
      return false; // Linea sobre el plan
    return false; // Linea paralela al plano
  }
  float d = n/den;
  if(d < 0.0f) // Va pal otro lao
    return false;
  if(intersection)
    *intersection = segment_origin + segment_dir * d;
  return true;
}

bool segmentIntersectPlane( const TPoint &segment_origin, const TPoint &segment_end, const TPoint &plane_point, const TPoint &plane_normal, TPoint *intersection ) {
  TPoint vopo = plane_point - segment_origin;
  float num = plane_normal.dot( vopo );
  TPoint pipo = segment_end - segment_origin;
  float den = plane_normal.dot( pipo );
  if( den == 0.0f ) {
    return false;
  }
  //if( den > 0 )
  //	return false; // le da por detrás
  float d = num / den;
  if(clamp(d, 0.0f, 1.0f) == d) {
    if(intersection) {
      TPoint dir = segment_end - segment_origin;
      //dir.normalize( );
      *intersection = segment_origin + dir * d;
    }
    return true;
  }
  return false;
}

// ---------------------------------------------------------------------
bool segmentIntersectSphere( const TPoint &segment_origin, const TPoint &segment_end, const TPoint &sphere_center, float radius, TPoint *intersection ) {
  TPoint closest = closestSegmentPoint( segment_origin, segment_end, sphere_center );

  TPoint dist_v = sphere_center - closest;
  float dist_length = dist_v.length( );
  if ( dist_length > radius ) {
    return false;
  }
  if ( intersection ) {
    //dist_v / dist_v.len() * (circ_rad - dist_v.len())
    TPoint intersect = dist_v / dist_length * ( radius - dist_length );
    *intersection = intersect;
  }
  return true;

}

// ---------------------------------------------------------------------
// Returns current_value + amount_changed not going beyond desired_value
// amount_changed is suposed to be a value like change_speed * delta_time
// change_speed > 0
float getChangingValue( float current_value, float desired_value, float amount_changed ) {
  if( current_value == desired_value )
    return desired_value;
  if( current_value < desired_value ) {
    current_value += amount_changed;
    if( current_value > desired_value )
      return desired_value;

  } else {
    current_value += amount_changed;
    if( current_value < desired_value )
      return desired_value;
  }

  return current_value;
}

float fast_sqrt( float number )
{


  long i;
  float x, y;
  const float f = 1.5F;

  x = number * 0.5F;
  y  = number;
  i  = * ( long * ) &y;
  i  = 0x5f3759df - ( i >> 1 );
  y  = * ( float * ) &i;
  y  = y * ( f - ( x * y * y ) );
  //y  = y * ( f - ( x * y * y ) );
  return number * y;
}

TPoint changeAxisHand( TPoint &p ) {
  return TPoint( p.x, p.z, p.y );
}

float lerp( float a, float b, float r ) {
  return a + (b-a) * r;
}

 */
