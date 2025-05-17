#include "platform.h"
#include "render/render.h"
#include "modules/module_render.h"
#include "resource.h"
#include <windows.h>
#include <windowsx.h>
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/ImGuizmo.h"

static ModuleRender* render_module = nullptr;

#define WINDOW_CLASS_NAME "App"
static DWORD window_style = WS_OVERLAPPEDWINDOW;
HWND hWnd;
bool user_wants_to_exit = false;
bool resizing_window = false;
VEC2 windows_size;
VEC2 mouse_cursor;

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

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

//-----------------------------------------------------------------------------
static LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {

  // Do we have a wndProc installed?
  if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
    return true;

  switch (msg) {

  case WM_SIZE: {
    UINT width = LOWORD(lParam);
    UINT height = HIWORD(lParam);
    if (width && height && !resizing_window) {
      dbg("Resized to %d x %d\n", width, height);
      RenderPlatform::resizeBackBuffer(width, height);
    }
    break;
  }

  case WM_MOUSEMOVE:
    mouse_cursor = VEC2((LONG)GET_X_LPARAM(lParam), (LONG)GET_Y_LPARAM(lParam));
    break;

  case WM_ENTERSIZEMOVE:
    resizing_window = true;
    break;

  case WM_EXITSIZEMOVE: {
    resizing_window = false;
    // Only when the resize finishes, notify the update to the ModuleRender,
    // which will update the deferred
    RECT rect;
    GetClientRect(hWnd, &rect);
    RenderPlatform::resizeBackBuffer(rect.right - rect.left, rect.bottom - rect.top);
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
  windows_size = VEC2((float)xres, (float)yres);

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

  {
    PROFILE_SCOPED_NAMED("ImGui.Beg");
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
  }

  RenderPlatform::beginFrame(++frame_id);
  int w, h;
  RenderPlatform::getBackBufferSize(&w, &h);
  render_module->generateFrame(w, h);

  {
    PROFILE_SCOPED_NAMED("ImGui.End");
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
  }
  RenderPlatform::swapFrames();
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {

  setDebugHandler(myDbgHandler);
  setFatalHandler(myFatalHandler);

  createWindow();

  if (!RenderPlatform::create(hWnd))
    return -1;

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
  io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
  ImGui::StyleColorsDark();
  if (!ImGui_ImplWin32_Init(hWnd))
    return -2;
  if (!ImGui_ImplDX11_Init(RenderPlatform::device, RenderPlatform::ctx))
    return -3;

  Modules::get().load();
  ModuleRender* render_module = (ModuleRender*)(Modules::get().getModule("render"));
  assert(render_module);

  while (true) {
    {
      PROFILE_SCOPED_NAMED("OS");
      while (processOSMsgs()) {}
      if (user_wants_to_exit)
        break;
    }

    PROFILE_BEGIN_FRAME();
    PROFILE_SCOPED_NAMED("Frame");
    Modules::get().update();
    generateFrame(render_module);
  }

  ImGui_ImplDX11_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();
  Modules::get().unload();
  RenderPlatform::destroy();

  return 0;
}