#pragma once 

void createAxis(Render::Mesh* mesh);
void createGrid(Render::Mesh* mesh, float sz = 1.f, int nsamples = 2, int nsubsamples = 5);
void createUnitWiredCube(Render::Mesh* mesh);
void createUnitWiredCircleXZ(Render::Mesh* mesh, int nsamples);
void createLine(Render::Mesh* mesh);
void createViewVolume(Render::Mesh* mesh);
void createQuadXYBasic(Render::Mesh* mesh);
void createQuadXZ(Render::Mesh* mesh, float sz = 1.0f);
void createQuadXY(Render::Mesh* mesh, float sz = 1.0f, VEC2 center_at = VEC2(0.f,0.f));
void createQuadXYTextured(Render::Mesh* mesh, float sz, int subdivs = 0, VEC2 center_at = VEC2(0.f,0.f));
void createUnitQuadXY(Render::Mesh* mesh);
void createUnitCube(Render::Mesh* mesh, float sz = 1.0f);
