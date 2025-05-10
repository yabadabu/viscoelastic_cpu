#include "platform.h"
#include "camera.h"

CCamera::CCamera()
  : aspect_ratio(1.0f)
{
  lookAt(VEC3(0, 0, 0), VEC3(0, 0, 1), VEC3(0, 1, 0));
  setProjectionParams(deg2rad(65.0f), 1.0f, 100.0f);
  setViewport(0, 0, 512, 512);
}

void CCamera::lookAt(const VEC3& src
  , const VEC3& dst
  , const VEC3& aux
) {
  // Save params
  position = src;
  target = dst;
  aux_up = aux;
  front = target - position;
  distance_to_target = front.length();
  assert( distance_to_target > 0.0f );

  front *= 1.0f / distance_to_target;
  left = aux_up.cross(front);
  left.normalize();

  up = front.cross(left);
  up.normalize();

  // update view
  view = MAT44::createLookAt(position, target, aux_up);
  updateViewProjection();
}

void CCamera::updateViewProjection() {
  view_projection = view * projection;
}

void CCamera::setViewport(int x0, int y0, int xres, int yres) {
  viewport.TopLeftX = (float)x0;
  viewport.TopLeftY = (float)y0;
  viewport.Width = (float)xres;
  viewport.Height = (float)yres;
  viewport.MinDepth = 0.f;
  viewport.MaxDepth = 1.f;

  aspect_ratio = viewport.Width / viewport.Height;
  if (is_ortho) {
    setProjectionOrtho(ortho_min.x, ortho_max.x, ortho_min.y, ortho_max.y, ortho_min.z, ortho_max.z);
  }
  else {
    setProjectionParams(fov_in_radians, znear, zfar);
  }
}

void CCamera::setProjectionParams(float new_fov_in_radians
  , float new_z_near, float new_z_far
) {

  fov_in_radians = new_fov_in_radians;
  znear = new_z_near;
  zfar = new_z_far;
  is_ortho = false;

  projection = MAT44::createPerspectiveFieldOfView(new_fov_in_radians, aspect_ratio, new_z_near, new_z_far);
  updateViewProjection();
}

void CCamera::setProjectionOrtho(float xmin, float xmax
  , float ymin, float ymax
  , float zmin, float zmax) {
  is_ortho = true;
  ortho_min = VEC3(xmin, ymin, zmin);
  ortho_max = VEC3(xmax, ymax, zmax);
  projection = MAT44::createOrthographicOffCenter(xmin, xmax, ymin, ymax, zmin, zmax);
  updateViewProjection();
}

bool CCamera::getScreenCoords(VEC3 world_pos, float* screen_x, float* screen_y) const {

  assert(screen_x);
  assert(screen_y);
  MAT44 vp = getViewProjection();
  VEC3 homo_space_coords = vp.transformCoord(world_pos);
  // -1 .. 1    homo_space_coords
  // 0 ... 2    + 1.0f
  // 0 ... 1    * 0.5f
  // 0 ... 800  * App.xres
  *screen_x = (homo_space_coords.x + 1.0f) * 0.5f * viewport.Width;
  *screen_y = (1.0f - (homo_space_coords.y)) * 0.5f * viewport.Height;

  // Return true if the z is inside the zrange of the camera
  float hz = (homo_space_coords.z);
  return hz >= 0.f && hz <= 1.f;
}

// -----------------------------------------------------------------
// (0.0, 0.0f) Upper - Left corner of the camera frustum
// (1.0, 1.0f) Lower - Right corner of the camera frustum
// (0.5, 0.5f) Center of the camera -> front
VEC3 CCamera::getRayDirection(float normalized_x_from_left_to_right, float normalized_y_from_top_to_bottom) const {
  float x = -((2.0f * normalized_x_from_left_to_right) - 1.0f);
  float y = ((2.0f * normalized_y_from_top_to_bottom) - 1.0f);
  float d = 1.0f / tanf(fov_in_radians * 0.5f);

  VEC3 ray_dir = front * d
    + left * x * aspect_ratio
    - up * y;
  ray_dir.normalize();
  return ray_dir;
}

VEC3 CCamera::getRayDirectionFromViewportCoords(float sx, float sy) const {
  float ux = (sx - viewport.TopLeftX) / (float)viewport.Width * sample_count;
  float uy = (sy - viewport.TopLeftY) / (float)viewport.Height * sample_count;
  return getRayDirection(ux, uy);
}

VEC3 CCamera::getHitXZ(float normalized_x_from_left_to_right, float normalized_y_from_top_to_bottom) const {
  VEC3 ray_dir = getRayDirectionFromViewportCoords(normalized_x_from_left_to_right, normalized_y_from_top_to_bottom );
  TRay ray(getPosition(), ray_dir);
  float t = ray.intersectsPlane(VEC3::axis_y, 0.0f);
  return ray.at(t);
}
