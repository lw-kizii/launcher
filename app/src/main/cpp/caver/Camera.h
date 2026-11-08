#ifndef LAWNCHER_CAMERA_H
#define LAWNCHER_CAMERA_H

#include "hook.h"
#include "types.h"
#include "lua.h"

typedef struct Camera Camera;
typedef struct CameraController CameraController;

static inline CameraController *cameraController_from_L(lua_State *L);

typedef struct Camera {
	char _pad0[0x10];
	Vector3 position;
	Quaternion rotation;
	Matrix4 view;
	Matrix4 projection;
	Matrix4 viewProjection;
	bool isOrtho;
	char _pad1[0x03];
	float aspect;
	float fov;
	float nearPlane;
	float farPlane;
	Matrix4 inverseProjection;
	Matrix4 inverseView;
	Matrix4 inverseViewProjection;
} Camera;

#include "CameraController.h"

static inline Camera *camera_from_L(lua_State *L) {
	CameraController *cc = cameraController_from_L(L);
	if (!cc) return NULL;
	return cc->camera;
}

#endif
