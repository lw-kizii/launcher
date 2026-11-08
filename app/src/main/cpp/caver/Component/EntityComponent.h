#ifndef LAWNCHER_ENTITYCOMPONENT_H
#define LAWNCHER_ENTITYCOMPONENT_H

#include "hook.h"
#include "PhysicsObjectState.h"
#include "Component.h"

typedef struct EntityComponent {
	void* vtable;
	char _pad0[archSplit(0x34, 0x60)];
	int facingDirection;
	char _pad1[archSplit(0x0, 0x4)];
	PhysicsObjectState physics; // unsure about this...
	char _pad2[archSplit(0x2c, 0x38)];
} EntityComponent; // sizeof(0xdc, 0x128)

static inline EntityComponent *entityComponent_get(SceneObject *obj) {
	return (EntityComponent *)component_fetch(obj, "EntityComponent");
}

#endif //LAWNCHER_ENTITYCOMPONENT_H
