#ifndef LAWNCHER_RENDERINGCONTEXT_H
#define LAWNCHER_RENDERINGCONTEXT_H

#include "hook.h"
#include "types.h"

typedef struct RenderingContext {
	int api; // don't know too much about this...
	char _pad0[archSplit(0x0c, 0x18)];
	void *programsBegin;
	void *programsEnd;
	Rectangle viewport;
	char _pad1[archSplit(0x08, 0x10)];
	float clearColor[4];
	bool blendingEnabled;
	char _pad2[archSplit(0x03, 0x07)];
	unsigned char color[4];
	char _pad3[archSplit(0x04, 0x08)];
	float alpha;
	bool depthTestEnabled;
	char _pad4[archSplit(0x03, 0x07)];
	Matrix4 matrix;
	char _pad5[archSplit(0x40, 0x50)];
} RenderingContext;

#endif
