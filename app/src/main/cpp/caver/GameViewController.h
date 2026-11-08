#ifndef LAWNCHER_GAMEVIEWCONTROLLER_H
#define LAWNCHER_GAMEVIEWCONTROLLER_H

#include "hook.h"
#include "GameState.h"
#include "lua.h"

typedef struct GameSceneController GameSceneController;

typedef struct GameViewController {
	char _pad0[archSplit(0x48, 0x88)];
	void *PlayerProfile;
	char _pad1[archSplit(0x0c, 0x18)];
	GameState *GameState;
	char _pad2[archSplit(0x0c, 0x18)];
	GameSceneController *GameSceneController;
	char _pad3[archSplit(0x04, 0x08)];
	void *GameSceneView;
	char _pad4[archSplit(0x40, 0x80)];
} GameViewController;

static inline GameViewController *gvc_from_L(lua_State *L) {
	lua_getglobal(L, "gameController");
	if (!lua_islightuserdata(L, -1)) return NULL;
	const void *gvc = lua_topointer(L, -1);
	lua_pop(L, 1);
	return (GameViewController *)gvc;
}

#endif
