#pragma once

struct ParticlesVec {
  std::vector<float> buf;
  float* x;
  float* y;
  float* z;
  inline VEC3 get(int i) const {
    return VEC3(x[i], y[i], z[i]);
  }
  inline void set(int i, const VEC3& v) {
    x[i] = v.x;
    y[i] = v.y;
    z[i] = v.z;
  }
  void resize(size_t new_size) {
    buf.resize(new_size * 3);
    x = buf.data();
    y = x + new_size;
    z = y + new_size;
  }
  void swap(ParticlesVec& other) {
    std::swap(buf, other.buf);
    std::swap(x, other.x);
    std::swap(y, other.y);
    std::swap(z, other.z);
  }
  void clearN(size_t n) {
    memset(x, 0x00, n * sizeof(float));
    memset(y, 0x00, n * sizeof(float));
    memset(z, 0x00, n * sizeof(float));
  }
  void add(int i, const VEC3& v) {
    x[i] += v.x;
    y[i] += v.y;
    z[i] += v.z;
  }
  inline void add(int i, float dx, float dy, float dz) {
    x[i] += dx;
    y[i] += dy;
    z[i] += dz;
  }
  void copyFrom(ParticlesVec& other, size_t num_particles) {
    memcpy(x, other.x, sizeof(float) * num_particles);
    memcpy(y, other.y, sizeof(float) * num_particles);
    memcpy(z, other.z, sizeof(float) * num_particles);
  }
};