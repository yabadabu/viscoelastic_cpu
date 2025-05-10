
namespace ImGui {

	IMGUI_API bool DragFloatEx(const char* title, float* addr_value, float speed, float v_min, float v_max);
	IMGUI_API bool DragFloat2Ex(const char* title, float v[2], float speed, float v_min, float v_max);
	IMGUI_API bool DragFloat3Ex(const char* title, float v[3], float speed, float v_min, float v_max);
	IMGUI_API bool DragFloat4Ex(const char* title, float v[4], float speed, float v_min, float v_max);
	IMGUI_API bool DragIntEx(const char* title, int* addr_value, float speed, int v_min, int v_max);

}
