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

CameraController *cameraController_from_L(lua_State *L);
CameraController *cameraController_get();

DL_SYMBOL_DECL(CameraController_Update, void, (CameraController *cc, float dt));
DL_SYMBOL_DECL(CameraController_FollowObject, void, (CameraController *cc, void *intrusive_object, Vector3 *offset));
DL_SYMBOL_DECL(CameraController_StopFollowing, void, (CameraController *cc));
DL_SYMBOL_DECL(CameraController_FocusAtPoint, void, (CameraController *cc, Vector3 *point, bool immediate));
DL_SYMBOL_DECL(CameraController_GotoTargetImmediately, void, (CameraController *cc));
DL_SYMBOL_DECL(CameraController_ResetFocus, void, (CameraController *cc));
DL_SYMBOL_DECL(CameraController_Rumble, void, (CameraController *cc));

#endif
