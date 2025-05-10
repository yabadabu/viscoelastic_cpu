#ifndef INC_SHADER_CONSTANTS
#define INC_SHADER_CONSTANTS

// Size needs to be multiple of 32 (for Metal)
struct CteCamera {
  MAT44  view_proj;
  VEC3   position;
  float  world_time;
  VEC3   forward;
  float  aspect_ratio;
  VEC3   left;
  float  dummy3;
  VEC3   up;
  float  dummy4;
};

// Size needs to be multiple of 32 
struct CteObject {
	MAT44 world;
	VEC4  color;
	int   mat_id;
	VEC3  dummy;
};

// -------------------------------------
#define ShadersSlotCamera        1
#define ShadersSlotObjects       2

#endif