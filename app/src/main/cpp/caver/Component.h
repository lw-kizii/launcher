#ifndef LAWNCHER_COMPONENT_H
#define LAWNCHER_COMPONENT_H

#include <string.h>
#include <stdio.h>
#include "hook.h"
#include "stdstring.h"
#include "SceneObject.h"
#include "log.h"

#define LOG_TAG "ComponentSystem"

/*
 * Components are present within SceneObjects and
 * usually define the behaviour and utilities of an object.
 *
 * They can be accessed via
 * `SceneObject_ComponentWithInterface` using an interface handle,
 * `component_fetch` using the component's name like `ModelComponent`.
 */
typedef struct Component {
	void *vtable; // 0, 0
	int unknown0; // 4, 8
	char _pad0[archSplit(0x0, 0x4)]; // 8, 12
	void *vtable2; // 8, 16
	void *dat; // 12, 24
	int flags; // 16, 32
	int flags1; // 20, 36
	SceneObject *object; // 24 (0x18), 40 (0x28)
	String label; // 28 (0x1c), 48 (0x30)
} Component; // sizeof(0x28, 0x48)
// this = operator_new(0x28); Component::Component(this);
// this_00 = operator_new(0x48); Component::Component(this_00);

DL_SYMBOL(
	SceneObject_ComponentWithInterface,
	"_ZNK5Caver11SceneObject22ComponentWithInterfaceEl",
	void*, (SceneObject *obj, void *interface)
);

static inline Component* component_fetch(SceneObject *obj, const char *component_name) {
	// _ZN5Caver[n][ComponentID]9InterfaceEv
	// _ZN5Caver14ModelComponent9InterfaceEv

	char interface_id[256];
	int len = (int)strlen(component_name);
	snprintf(interface_id, sizeof(interface_id), "_ZN5Caver%d%s9InterfaceEv", len, component_name);
	void *interface = swordigo_dlsym(interface_id);
	if (!interface) return NULL;
	Component *comp = SceneObject_ComponentWithInterface(obj, interface);
	if (!comp) return NULL;
	LOGD("Fetched a component: %s", component_name);

	return comp;
}

#endif //LAWNCHER_COMPONENT_H
