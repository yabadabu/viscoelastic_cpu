#ifndef INC_RANDOMS_H_
#define INC_RANDOMS_H_

#include <cstdlib>

// ------------------------------------------------------------------------
// https://www.youtube.com/watch?v=LWFzPP8ZbdU&feature=emb_rel_end
inline constexpr unsigned int SquirrelNoise5( int positionX, unsigned int seed = 0 ) {
  constexpr unsigned int SQ5_BIT_NOISE1 = 0xd2a80a3f; // 11010010101010000000101000111111
  constexpr unsigned int SQ5_BIT_NOISE2 = 0xa884f197; // 10101000100001001111000110010111
  constexpr unsigned int SQ5_BIT_NOISE3 = 0x6C736F4B; // 01101100011100110110111101001011
  constexpr unsigned int SQ5_BIT_NOISE4 = 0xB79F3ABB; // 10110111100111110011101010111011
  constexpr unsigned int SQ5_BIT_NOISE5 = 0x1b56c4f5; // 00011011010101101100010011110101
  unsigned int mangledBits = (unsigned int) positionX;
  mangledBits *= SQ5_BIT_NOISE1;
  mangledBits += seed;
  mangledBits ^= (mangledBits >> 9);
  mangledBits += SQ5_BIT_NOISE2;
  mangledBits ^= (mangledBits >> 11);
  mangledBits *= SQ5_BIT_NOISE3;
  mangledBits ^= (mangledBits >> 13);
  mangledBits += SQ5_BIT_NOISE4;
  mangledBits ^= (mangledBits >> 15);
  mangledBits *= SQ5_BIT_NOISE5;
  mangledBits ^= (mangledBits >> 17);
  return mangledBits;
}

inline uint32_t Get1dNoiseUInt(uint32_t pos, uint32_t seed = 0) {
	return SquirrelNoise5( pos, seed );
}

inline uint32_t Get2dNoiseUInt(uint32_t posX, uint32_t posY, uint32_t seed = 0) {
  constexpr uint32_t PRIME_NUMBER = 198491317;
  return Get1dNoiseUInt(posX + (posY * PRIME_NUMBER), seed);
}

inline uint32_t Get3dNoiseUInt(uint32_t posX, uint32_t posY, uint32_t posZ, uint32_t seed = 0) {
  constexpr uint32_t PRIME1 = 198491317;
  constexpr uint32_t PRIME2 = 6542989;
  return Get1dNoiseUInt(posX + (posY * PRIME1) + (posZ * PRIME2), seed);
}

inline float unitRandom() {
  return rand() / static_cast<float>(RAND_MAX);
}

inline float randomBetween(float a, float b) {
  return a + (b - a) * unitRandom();
}

inline float randomAngle() {
  return randomBetween((float)-M_PI, (float)M_PI);
}

inline int random(int vmin, int vmax) {
  if (vmin == vmax)
    return vmin;
  return vmin + (rand() % (vmax - vmin));
}

// 0 and nvalues-1
inline int randomInt(int nvalues) {
  assert(nvalues > 0);
  if (nvalues == 1)
    return 0;
  else
    return rand() % nvalues;
}

inline bool randomBool() {
  return (bool)((rand() % 2) != 0);
}

inline void setSeedOfRandoms(int new_seed) {
  srand(new_seed);
}

inline void randomSeed() {
  srand((unsigned int)time(NULL));
}

// ------------------------------------------------------------------------
inline float unitRandomWithSeed(int* seed) {
  union {
    float fres;
    unsigned int ires;
  };
  seed[0] *= 16807;
  ires = ((((unsigned int)seed[0]) >> 9) | 0x3f800000);
  return fres - 1.0f;
}

inline float uniformDistributedSample1D(int n, float base) {
  float g = 1.6180339887498948482f;
  float a1 = 1.0f / g;
  float v = (base + a1 * n);
  return v - (int)v;
}

// The seed is the base, which should be a number between 0 and 1
// the n is a seq number, starting at zero.
inline VEC2 uniformDistributedSample2D(int n, float base) {
  float g = 1.32471795724474602596f;
  float a1 = 1.0f / g;
  float a2 = 1.0f / (g * g);
  VEC2 t = VEC2(base + a1 * n, base + a2 * n);
  t.x = t.x - (int)t.x;
  t.y = t.y - (int)t.y;
  return t;
}

inline VEC3 uniformDistributedSample3D(int n, float base) {
  float g = 1.2207440846057594736f;
  float a1 = 1.0f / g;
  float a2 = 1.0f / (g * g);
  float a3 = 1.0f / (g * g * g);
  VEC3 t = VEC3(base + a1 * n, base + a2 * n, base + a3 * n);
  t.x = t.x - (int)t.x;
  t.y = t.y - (int)t.y;
  t.z = t.z - (int)t.z;
  return t;
}

// n between 0 and nsamples-1
// phy_phase between 0 and 1
inline VEC3 randomSphereDirection(int n, int nsamples, float phy_phase) {
  static const float phy = (1.0f + sqrtf(5.0f)) * 0.5f;
  float x = (float)n / (float)phy;
  float y = (float)n / (float)nsamples;
  float yaw = 2 * M_PI * x;
  yaw += phy_phase * M_PI * 2.0f;     // Extra offset
  float pitch = acosf(2 * y - 1) - M_PI * 0.5f;
  return getVectorFromYawAndPitch(yaw, pitch);
}

struct TRandomSequence {

  TRandomSequence(unsigned int aseed = 12351251) : seed(aseed) { }

  unsigned int getSeed() const {
    return seed;
  }
  void setSeed(unsigned int new_seed) {
    seed = new_seed;
  }

  unsigned int rand() {
    seed *= 16807;
    seed = ((((unsigned int)seed) >> 9) | 0x3f800000);
    return seed;
  }

  float urandom() {
    union {
      float fres;
      unsigned int ires;
    };
    seed *= 16807;
    ires = ((((unsigned int)seed) >> 9) | 0x3f800000);
    return fres - 1.0f;
  }

  int between(int vmin, int vmax) {
    if (vmin == vmax)
      return vmin;
    return vmin + (this->rand() % (vmax - vmin));
  }

  float between(float vmin, float vmax) {
    return vmin + (vmax - vmin) * urandom();
  }

  VEC3 between(VEC3 vmin, VEC3 vmax) {
    VEC3 ur = VEC3(urandom(), urandom(), urandom());
    return vmin + (vmax - vmin) * ur;
  }

  VEC4 between(VEC4 vmin, VEC4 vmax) {
    VEC4 ur = VEC4(urandom(), urandom(), urandom(), urandom());
    return vmin + (vmax - vmin) * ur;
  }

private:
  unsigned int seed;
};


#endif
