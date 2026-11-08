#ifndef LAWNCHER_TEXTURELIBRARY_H
#define LAWNCHER_TEXTURELIBRARY_H

#include "hook.h"
#include "stdstring.h"

typedef struct TextureLibrary {
	void *vtable;
	void *texturesBegin;
	void *texturesEnd;
	char _pad0[archSplit(0x08, 0x10)];
	long long someCounter;
	char _pad1[archSplit(0x08, 0x10)];
	void *unusedList;
	char _pad2[archSplit(0x00, 0x08)];
} TextureLibrary;

#endif
