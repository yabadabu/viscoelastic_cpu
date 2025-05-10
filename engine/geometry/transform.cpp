#include "platform.h"
#include "transform.h"

void TTransform::interpolateTo(const TTransform& target, float amount_of_target) {
  assert(amount_of_target >= 0 && amount_of_target <= 1.f);
  position += (target.position - position) * amount_of_target;
  rotation = QUAT::slerp(rotation, target.rotation, amount_of_target);
  scale += (target.scale - scale) * amount_of_target;
}

TTransform TTransform::combineWith(const TTransform& delta) const {
  VEC3 abs_delta_pos = rotation.rotate(delta.position);
  TTransform new_tmx;
  new_tmx.position = position + abs_delta_pos * scale;
  new_tmx.rotation = delta.rotation * rotation;
  new_tmx.scale = scale * delta.scale;
  return new_tmx;
}

void TTransform::lookAt(VEC3 new_target, VEC3 new_up_aux) {
  MAT44 view = MAT44::createLookAt(position, position - (new_target - position), new_up_aux);
  rotation = QUAT::createFromRotationMatrix(view).inverted();
}

MAT44 TTransform::asMatrix() const {
  MAT44 m = MAT44::createFromQuaternion(rotation);
  if (scale.x != 1.f || scale.y != 1.0f || scale.z != 1.0f) {
    m.x *= scale.x;
    m.y *= scale.y;
    m.z *= scale.z;
  }
  m.translation(position);
  return m;
}

TTransform TTransform::inverse() const {
  TTransform inv;
  inv.rotation = rotation.inverted();
  //zero = p0 + r0 ( p1 ) * s0;
  // -p0 = r0(p1) * s0;
  // -p0 / s0 = r0(p1);
  inv.scale = VEC3(1.0f / scale.x, 1.0f / scale.y, 1.0f / scale.z);
  inv.position = inv.rotation.rotate( -position ) * inv.scale;
  return inv;
}

// ---------------------------
void TTransform::fromMatrix(MAT44 mtx) {

  VEC3 new_pos(mtx.w.x, mtx.w.y, mtx.w.z);
  setPosition(new_pos);

  scale.x = mtx.x.length();
  scale.y = mtx.y.length();
  scale.z = mtx.z.length();

  // Normalize axis to have the CreateFromRotationMatrix work properly
  MAT44 mtx_no_scale = mtx;
  mtx_no_scale.x *= 1.0f / mtx.x.length();
  mtx_no_scale.y *= 1.0f / mtx.y.length();
  mtx_no_scale.z *= 1.0f / mtx.z.length();
  QUAT new_rot = QUAT::createFromRotationMatrix(mtx_no_scale);
  setRotation(new_rot);
}


// ---------------------------
void TTransform::getAngles(float* yaw, float* pitch, float* roll) const {
  VEC3 f = getFront();
  getYawPitchFromVector(f, yaw, pitch);

  // If requested...
  if (roll) {
    VEC3 roll_zero_left = VEC3(0, 1, 0).cross(getFront());
    VEC3 roll_zero_up = VEC3(getFront()).cross(roll_zero_left);

    VEC3 my_real_left = getLeft();
    float rolled_left_on_up = my_real_left.dot(roll_zero_up);
    float rolled_left_on_left = my_real_left.dot(roll_zero_left);
    *roll = atan2f(rolled_left_on_up, rolled_left_on_left);
  }
}

VEC3 TTransform::transformCoord(VEC3 p) const {
	return rotation.rotate(p) * scale + position;
}

VEC3 TTransform::transformDir(VEC3 p) const {
	return rotation.rotate(p);
}

void TTransform::setAngles(float new_yaw, float new_pitch, float new_roll) {
  rotation = QUAT::createFromYawPitchRoll(new_yaw, -new_pitch, new_roll);
}
