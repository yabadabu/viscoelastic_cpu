#pragma once

class CameraController;
class CCamera;

struct AppMsgApplicationStarts {
};

struct AppMsgSetMainCameraController {
	CameraController* controller = nullptr;
	const char* name = nullptr;
};

struct AppMsgActivateCamera {
	CCamera* camera = nullptr;
	int   index = 0;		// 0:3D, 1:2D
	float w = 0;
	float h = 0;
};

