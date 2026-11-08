#ifndef LAWNCHER_BINDINGVALUE_H
#define LAWNCHER_BINDINGVALUE_H

#include "hook.h"
#include "stdstring.h"
#include "types.h"

typedef struct BindingValue {
	int type;
	char _pad0[archSplit(0x0, 0x4)];
	void* value;
	void* value_count;
	String* description;
	void* description_count;
} BindingValue; // sizeof(0x14, 0x28)

typedef struct Binding {
	String name;
	BindingValue value;
} Binding; // sizeof(0x2c, 0x40)

#endif //LAWNCHER_BINDINGVALUE_H
