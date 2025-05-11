#pragma once

struct TBuffer;

// -------------------------------------------------------
struct ENGINE_API Module {
  bool is_loaded = false;
  Module();
  virtual void load() { }
  virtual void unload() { }
  virtual int getPriority() const { return 50; }
  virtual const char* getName() const = 0;

  virtual void onRender3D() {}
  virtual void renderInMenu() {}
  virtual void update() { }

  void setLoaded(bool new_is_loaded ) {
    if (is_loaded == new_is_loaded)
      return;
    is_loaded = new_is_loaded;
    if (new_is_loaded) 
      load();
    else 
      unload();
  }
};

// -------------------------------------------------------
class Modules {

  static constexpr int max_modules = 64;

  // Using a simple array to avoid the global ctor initialization pitfall
  // where the modules are constructed before the Modules manager
  Module*  modules_registered[max_modules] = {};
  int      nmodules_registered = 0;
  bool     modules_running = false;

public:
  void registerModule(Module* new_module);
  
  static Modules& get();
  void load();
  void update();
  void unload();
  void onRender3D();
  void renderInMenu();

  Module* getModule(const char* mod_name);
};

struct FrameTime {
  float  elapsed = 0.0f;
  double now = 0.0f;
  size_t frame_id = 1;
};
ENGINE_API const FrameTime& frameTime();

