#include "platform.h"
#include "module.h"
#include "time/timer.h"
#include <algorithm>

static FrameTime frame_time;

const FrameTime& frameTime() { return frame_time; }

Module::Module() {
  Modules::get().registerModule(this);
}

void Modules::registerModule(Module* new_module) {
  if (!modules_running)
    modules_registered[nmodules_registered++] = new_module;
}

Modules& Modules::get() {
  static Modules modules_singleton;
  return modules_singleton;
}

void Modules::update() {
  PROFILE_SCOPED_NAMED("Modules::update");

  // Update time.
  static TTimer tm;
  static TTimer tm2;
  frame_time.elapsed = (float)tm2.elapsed();
  frame_time.now += frame_time.elapsed;
  frame_time.frame_id++;
  const float max_elapsed_time = 5.0f / 60.0f;
  if(frame_time.elapsed > max_elapsed_time) frame_time.elapsed = max_elapsed_time;
}

static bool sortByPrioriry( const Module* m1, const Module* m2 ) {
  return m1->getPriority() < m2->getPriority();
}

void Modules::load() {
  PROFILE_SCOPED_NAMED("Modules::Load");
  modules_running = true;

  std::sort( modules_registered, modules_registered + nmodules_registered, sortByPrioriry );

  for( int i = 0; i<nmodules_registered; ++i ) {
    Module* m = modules_registered[i];
    if( !m->startsLoaded() )
      continue;
    PROFILE_SCOPED_NAMED(m->getName());
    dbg( "Loading module %s\n", m->getName());
    modules_registered[i]->setLoaded(true);
  }

}

void Modules::unload() {
  dbg( "There are %d modules registered\n", nmodules_registered);
  // Process the events in reverse order
  for( int i = 0; i<nmodules_registered; ++i ) {
    assert( nmodules_registered - 1 - i >= 0 );
    Module* m = modules_registered[nmodules_registered - 1 - i];
    dbg( "Unloading module %s\n", m->getName());
    modules_registered[i]->setLoaded(false);
  }
  dbg( "Modules::unload completed\n");
}

Module* Modules::getModule(const char* mod_name) {
  for( int i = 0; i<nmodules_registered; ++i ) {
    Module* m = modules_registered[i];
    if( strcmp( m->getName(), mod_name ) == 0 )
      return m;
  }
  fatal( "Failed to find module '%s'\n", mod_name );
  return nullptr;
}


