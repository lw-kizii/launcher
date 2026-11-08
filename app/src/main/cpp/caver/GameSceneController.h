#ifndef LAWNCHER_GAMESCENECONTROLLER_H
#define LAWNCHER_GAMESCENECONTROLLER_H

#include "hook.h"
#include "SceneObject.h"
#include "CharacterState.h"
#include "lua.h"
#include "GameViewController.h"

typedef struct GameSceneController {
	void* vtable;
	void* CharacterState;
	char _pad0[archSplit(0x4, 0x10)];
	void* Scene;
	char _pad1[archSplit(0x94, 0xb0)];
	SceneObject* hero;
	void* CharControllerComponent;
	void* EntityComponent;
	void* HealthComponent;
	void* ManaComponent;
	void* HeroEquipmentManager;
	char _pad2[archSplit(0x24, 0x44)];
	Vector3 spawnPosition;
} GameSceneController;

static inline GameSceneController *gsc_from_L(lua_State *L) {
	GameViewController *gvc = gvc_from_L(L);
	return gvc->GameSceneController;
}

#endif
