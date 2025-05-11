#pragma once

// ----------------------------------------
struct TTransform {     // 1
  QUAT  rotation;
  VEC3  position;
  VEC3  scale = VEC3(1,1,1);

  TTransform() {}
  TTransform(VEC3 npos) : position(npos) {}
  TTransform(VEC3 npos, QUAT nrot, VEC3 nscale = VEC3(1,1,1)) : rotation(nrot), position(npos), scale(nscale) {}

  VEC3 getPosition() const { return position; }
  void setPosition(VEC3 new_pos) { position = new_pos; }
  QUAT getRotation() const { return rotation; }
  void setRotation(QUAT new_rot) { rotation = new_rot; }

  VEC3 getFront() const {
    MAT44 m = rotation.asMatrix();
    return VEC3(m.z.x, m.z.y, m.z.z); //r[2];
  }

  VEC3 getLeft() const {
    MAT44 m = rotation.asMatrix();
    return VEC3(m.x.x, m.x.y, m.x.z); //r[0];
  }

  VEC3 getUp() const {
    MAT44 m = rotation.asMatrix();
    return VEC3(m.y.x, m.y.y, m.y.z); //r[1];
  }

  VEC3 getScale() const { return scale; }
  void setScale(VEC3 new_scale) { scale = new_scale; }

    // Aim the transform to a position instantly
  void lookAt(VEC3 new_target, VEC3 new_up_aux);

  MAT44 asMatrix() const;
  void fromMatrix(MAT44 mtx);

  void interpolateTo(const TTransform& target, float amount_of_target);
  TTransform combineWith(const TTransform& delta) const;
  void       getAngles(float* yaw, float* pitch, float* roll = nullptr) const;
  void setAngles(float new_yaw, float new_pitch, float new_roll = 0.f);
  TTransform inverse() const;

  bool debugInMenu();

  VEC3 transformCoord(VEC3 p) const;
  VEC3 transformDir(VEC3 p) const;
};

