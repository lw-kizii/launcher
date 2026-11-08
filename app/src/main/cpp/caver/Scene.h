#ifndef LAWNCHER_SCENE_H
#define LAWNCHER_SCENE_H

#include "hook.h"
#include "ProgramState.h"
#include "stdstring.h"
#include "types.h"
#include "Camera.h"

typedef struct Scene Scene; // TODO: needs to be remapped!!
// hitboxes should be at 0x1c4,

static inline Scene *scene_from_L(lua_State *L) {
	lua_getglobal(L, "scene");
	if (!lua_islightuserdata(L, -1)) return NULL;
	const void *scene = lua_topointer(L, -1);
	return (Scene *)scene;
}

#endif
