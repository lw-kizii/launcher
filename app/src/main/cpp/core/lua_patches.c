#include <pthread.h>
#include "hook.h"
#include "log.h"
#include "lua.h"
#include <stdbool.h>

#define LOG_TAG "ProgramPatch"

#ifdef __arm__ // This should work on arm now...

HOOK_SYMBOL(
	LoadIntoState,
	"_ZNK5Caver7Program13LoadIntoStateEP9lua_State",
	int, (void *this, lua_State *L)
) {
	void **string_px = $(void*, this, 0x0c, 0x18);
	void **string_pn = $(void*, this, 0x10, 0x20);
	void **bytes_px = $(void*, this, 0x14, 0x28);
	void **bytes_pn = $(void*, this, 0x18, 0x30);

	if (string_px && *string_px != NULL) {
		void *saved_px = *bytes_px;
		void *saved_pn = *bytes_pn;

		*bytes_px = *string_px;
		*bytes_pn = *string_pn;

		int ok = orig_LoadIntoState(this, L);

		*bytes_px = saved_px;
		*bytes_pn = saved_pn;

		return ok;
	}

	return orig_LoadIntoState(this, L);
}

#endif

HOOK_SYMBOL(
	ProgramState_Update,
	"_ZN5Caver12ProgramState6UpdateEf",
	void, (void *ps, float dt)
) {
	if (!ps) {
		LOGE("ProgramState is NULL?");
		return;
	}

	lua_State *L = *$(lua_State *, ps, 0x0, 0x0);
	char aborted = *$(char, ps, 0x33, 0x5b);
	orig_ProgramState_Update(ps, dt);

	aborted = *$(char, ps, 0x33, 0x5b);
	if (aborted && L) {
		if (lua_gettop(L) > 0 && lua_isstring(L, -1)) {
			const char *err = lua_tostring(L, -1);
			LOGE("%p encountered error: %s", ps, err);
		}
	}
}
