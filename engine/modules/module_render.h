#pragma once

#include "module.h"

struct ModuleRender : public Module {
  const char* getName() const override { return "render"; }
  int getPriority() const override { return 10; }
  void load() override;
  void unload() override;
  void renderInMenu() override;
  void generateFrame( int w, int h );
};

#include "../../data/shaders/constants.h"
