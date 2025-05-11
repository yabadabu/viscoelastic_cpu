#pragma once

#include "geometry.h"

class CCamera {

protected:

  // View 
  VEC3   position;
  VEC3   target;
  VEC3   aux_up;
  float	 distance_to_target = 1.0f;
  MAT44  view;

  VEC3   front;
  VEC3   left;
  VEC3   up;

  TViewport viewport;

  MAT44    projection;
  MAT44    view_projection;		//

  VEC3   ortho_min, ortho_max;
  bool   is_ortho = false;

public:

  // Prespective parameters
  float    fov_in_radians = deg2rad( 65 );	// Vertical fov
  float    aspect_ratio = 1.0f;
  float    znear = 0.02f;
  float    zfar = 100.0f;
  int      sample_count = 1;

  void updateViewProjection();

public:

  CCamera();

  // View 
  void lookAt(const VEC3& src
    , const VEC3& dst
    , const VEC3& aux
  );

  const VEC3& getPosition()   const { return position; }
  const VEC3& getTarget()     const { return target; }
  const MAT44& getView()       const { return view; }
  const MAT44& getProjection() const { return projection; }
  const MAT44& getViewProjection() const { return view_projection; }
  const VEC3& getFront()     const { return front; }
  const VEC3& getLeft()       const { return left; }
  VEC3  getRight()      const { return -left; }
  const VEC3& getUp()        const { return up; }
  float getDistanceToTarget()        const { return distance_to_target; }

  void  setViewport(int x0, int y0, int xres, int yres);
  TViewport getViewport() const {
    return viewport;
  }

  // Proj
  void  setProjectionParams(float new_fov_in_radians
    , float new_z_near, float new_z_far
  );
  void  setProjectionOrtho(float xmin, float xmax
    , float ymin, float ymax
    , float zmin, float zmax);

  float getZNear() const { return znear; }
  float getZFar() const { return zfar; }
  float getAspectRatio() const { return aspect_ratio; }
  float getFov() const { return fov_in_radians; }

  // Returns true if the world_pos is inside the z range of the view frustum 
  bool getScreenCoords(VEC3 world_pos, float* x, float* y) const;

  VEC3 getRayDirection(float normalized_x_from_left_to_right, float normalized_y_from_top_to_bottom) const;
  VEC3 getRayDirectionFromViewportCoords(float x, float y) const;
  VEC3 getHitXZ(float normalized_x_from_left_to_right, float normalized_y_from_top_to_bottom) const;

};

class CameraController {
public:
    virtual ~CameraController() {} 
    virtual void updateCamera( CCamera* camera, float dt) = 0;
    virtual void onControllerActivated() { }
    virtual void onControllerDeActivated() { }
};


