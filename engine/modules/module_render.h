#pragma once

#include "module.h"

struct ModuleRender : public Module {
  const char* getName() const override { return "render"; }
  int getPriority() const override { return 10; }
  bool startsLoaded() const override { return true; }
  void load() override;
  void unload() override;
  void generateFrame( int w, int h );

private:
  void generateLightMap();
};

#include "../../data/shaders/constants.h"

CteObject* getCteObjectsData(uint32_t idx = 0 );
uint32_t allocCteObjects( uint32_t count );
void      freeCteObjects( uint32_t first, uint32_t count );
