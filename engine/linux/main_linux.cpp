#include "platform.h"
#include "render/render.h"
#include "modules/module_render.h"

int main( int argc, char** argv ) {
  	ImGui::CreateContext();
  	Modules::get().load();
  	for( int i=0; i<100; ++i ) {
  		PROFILE_BEGIN_FRAME();
  		Modules::get().update();
  		if( i == 80 )
  			PROFILE_START_CAPTURING(5);
  	}
  	Modules::get().unload();
  	ImGui::DestroyContext();
	return 0;
}