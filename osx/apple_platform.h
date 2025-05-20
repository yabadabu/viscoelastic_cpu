#pragma once

#ifdef __cplusplus
extern "C" {
#endif

	// -----------------------------------------
	struct RenderArgs {
		void* renderPassDescriptor;
		void* drawable;
		void* view;
		int   width;
		int   height;
	};

	void renderOpen( void* view);
	void renderFrame( struct RenderArgs* args );
	void renderClose();

	// -----------------------------------------
	void inputSend( const char* event_name, float x, float y );
  void runOSCommand( const char* cmd, void* args );

#ifdef __cplusplus
}
#endif
