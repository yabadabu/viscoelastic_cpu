#include "platform.h"
#include "render/render.h"
#include "modules/module_render.h"
#include "./resource.h"
#include <windows.h>
#include <windowsx.h>

static ModuleRender* render_module = nullptr;

#define WINDOW_CLASS_NAME "App"
static DWORD window_style = WS_OVERLAPPEDWINDOW;
HWND hWnd;
bool user_wants_to_exit = false;
bool resizing_window = false;
bool timer_installed = false;
VEC2 windows_size;

void resizeAppWindow(int w, int h) {
	RECT rect = { 0, 0, w, h };
	::AdjustWindowRect(&rect, window_style, FALSE);
	SetWindowPos(hWnd, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOMOVE);
	windows_size = VEC2((float)w, (float)h);
}

void myDbgHandler(const char* txt) {
  ::OutputDebugString(txt);
  if (0) {
    FILE* f = fopen("trace.txt", "a+");
    if (f) {
      fprintf(f, "%s", txt);
      fclose(f);
    }
  }
}

void myFatalHandler(const char* txt) {
  myDbgHandler(txt);
  if (MessageBox(nullptr, txt, "Error", MB_RETRYCANCEL) == IDCANCEL) {
    __debugbreak();
  }
}

int translateKey(WPARAM winKey) {
	//if( winKey >= ' ' && winKey <= 'Z' )
	//	return (int)winKey;
	//switch( winKey ) {
	//case VK_SHIFT: return KeyShift;
	//case VK_CONTROL: return KeyControl;
	//case VK_ESCAPE: return KeyEsc;
	//default:
	//	break;
	//}
	return 0;
}

//-----------------------------------------------------------------------------
static LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {

	switch (msg) {
	//case WM_KEYDOWN: {
	//	// Avoid autorepeat and strange keycodes
	//	int engine_key = translateKey(wParam);
	//	if (!(lParam & 0x40000000) && engine_key)
	//		inputSend("key.dw", (float)engine_key, 0.0f);
	//	break; }

	//case WM_KEYUP: {
	//	if( int engine_key = translateKey(wParam) )
	//		inputSend("key.up", (float)engine_key, 0);
	//	break; }


	case WM_SIZE: {
		UINT width = LOWORD(lParam);
		UINT height = HIWORD(lParam);
		if (width && height && !resizing_window) {
			dbg("Resized to %d x %d\n", width, height);
			//RenderPlatform::resizeBackBuffer(width, height);
		}
		break; }

	case WM_ENTERSIZEMOVE:
		resizing_window = true;
		break;

	case WM_EXITSIZEMOVE: {
		resizing_window = false;
		// Only when the resize finishes, notify the update to the ModuleRender,
		// which will update the deferred
		RECT rect;
		GetClientRect(hWnd, &rect);
		//RenderPlatform::resizeBackBuffer(rect.right - rect.left, rect.bottom - rect.top);
		windows_size.x = (float)(rect.right - rect.left);
		windows_size.y = (float)(rect.bottom - rect.top);
		break;
	}

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}


void createWindow() {

	int xres = 1512;
	int yres = 960;
	int xmin = 100;
	int ymin = 100;

	// Register the window class
	WNDCLASSEX wc = { sizeof(WNDCLASSEX),    CS_CLASSDC, MsgProc, 0L,   0L,
		  GetModuleHandle(nullptr), nullptr, LoadCursor(nullptr, IDC_ARROW),    nullptr, nullptr,
		  WINDOW_CLASS_NAME,     nullptr };
	wc.hIcon = LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_ICON1));
	wc.hIconSm = LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_ICON1));
	RegisterClassEx(&wc);

	RECT rect = { 0, 0, xres, yres };
	::AdjustWindowRect(&rect, window_style, FALSE);

	// Create the application's window
	int w = rect.right - rect.left;
	int h = rect.bottom - rect.top;
	hWnd = ::CreateWindow(WINDOW_CLASS_NAME, "App", window_style, xmin, ymin, w, h, nullptr, nullptr,
		wc.hInstance, nullptr);
	windows_size = VEC2( (float)xres, (float)yres );

	ShowWindow(hWnd, SW_SHOWDEFAULT);
	UpdateWindow(hWnd);
}

bool processOSMsgs() {
	MSG msg;
	ZeroMemory(&msg, sizeof(msg));
	if (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if (msg.message == WM_QUIT)
			user_wants_to_exit = true;
		return true;
	}
	return false;
}

void generateFrame(ModuleRender* render_module) {
	static uint32_t frame_id = 0;
	RenderPlatform::beginFrame( ++frame_id );
	int w,h;
	RenderPlatform::getBackBufferSize( &w, &h );
	render_module->generateFrame( w, h );
	RenderPlatform::swapFrames();
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {

  setDebugHandler(myDbgHandler);
  setFatalHandler(myFatalHandler);

  createWindow();

  if( !RenderPlatform::create( hWnd ) )
		return -1;
  Modules::get().load();
  ModuleRender* render_module = (ModuleRender*)( Modules::get().getModule("render"));
  assert(render_module);
  
  while (!user_wants_to_exit) {
	  while(processOSMsgs()) { }

	  Modules::get().update();
	  generateFrame( render_module );
  }

  Modules::get().unload();
  RenderPlatform::destroy();

  return 0;
}