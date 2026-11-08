#ifndef LAWNCHER_PLAYERPROFILE_H
#define LAWNCHER_PLAYERPROFILE_H

#include <stdlib.h>
#include <string.h>
#include "hook.h"
#include "stdstring.h"
#include "log.h"

typedef struct PlayerProfile {
	char _pad0[archSplit(0x0c, 0x18)];
	String Identifier;
	char _pad1[archSplit(0x20, 0x28)];
	void *lastPlayedTime; // uses DateTime, I need to decompile that later
	char _pad2[archSplit(0x40, 0x50)];
	int someFlag;
	char _pad3[archSplit(0x28, 0x30)];
	void *countersBegin;
	void *countersEnd;
	char _pad4[archSplit(0x10, 0x18)];
} PlayerProfile;

#endif //LAWNCHER_PLAYERPROFILE_H
