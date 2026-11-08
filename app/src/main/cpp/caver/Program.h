#ifndef LAWNCHER_PROGRAM_H
#define LAWNCHER_PROGRAM_H

#include "hook.h"
#include "stdstring.h"

typedef struct Program {
	String source;
	void* string;
	void* string_count;
	void* bytecode;
	void* bytecode_count;
} Program; // sizeof(0x1c, 0x38)

#endif //LAWNCHER_PROGRAM_H
