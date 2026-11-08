#ifndef LAWNCHER_CAMERACONTROLLER_H
#define LAWNCHER_CAMERACONTROLLER_H

#include "hook.h"
#include "types.h"
#include "Camera.h"
#include "SceneObject.h"
#include "lua.h"

typedef struct CameraController {
	int flags;
	Vector3 lookOffset;
	Vector3 targetPos;
	float lerpFactor;
	Vector3 focusPos;
	float zoomLerp;
	Vector3 currentPos;
	Vector3 currentFocus;
	Vector3 up;
	char _pad0[archSplit(0x00, 0x04)]; // some float on 64 bit?
	Camera *camera;
	void *cameraRef;
	char _pad1[archSplit(0x00, 0x00)];
	SceneObject *followObject;
	void *followShape;
	Vector3 followOffset;
	Rectangle followRect;
	float rumbleTime;
	float rumble;
} CameraController;

static inline CameraController *cameraController_from_L(lua_State *L) {
	lua_getglobal(L, "cameraController");
	if (!lua_islightuserdata(L, -1)) return NULL;
	const void *cc = lua_topointer(L, -1);
	lua_pop(L, 1);
	return (CameraController *)cc;
}

#endif
