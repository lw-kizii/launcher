#ifndef LAWNCHER_SCENEOBJECT_H
#define LAWNCHER_SCENEOBJECT_H

#include "hook.h"
#include "stdstring.h"
#include "types.h"

typedef struct SceneObject {
	void *vtable;
	int refCount;
	char _pad0[archSplit(0x08, 0x10)];
	void *Scene;
	char _pad1[archSplit(0x04, 0x08)];
	struct SceneObject *Parent;
	char _pad2[archSplit(0x04, 0x08)];
	float Scaling;
	char _pad3[archSplit(0x0c, 0x0c)];
	String Identifier;
	char _pad4[archSplit(0x04, 0x18)];
	Vector3 Position;
	char _pad5[archSplit(0x0c, 0x0c)];
	Rectangle LocalAABB;
	char _pad6[archSplit(0x5c, 0x50)];
	bool Hidden;
	char _pad7[archSplit(0x27, 0x33)];
} SceneObject;

#endif
