#include <pthread.h>
#include "hook.h"
#include "log.h"
#include "lua.h"
#include <stdbool.h>

#define LOG_TAG "ProgramPatch"

#ifdef __arm__            // 32-bit only

HOOK_SYMBOL(
	LoadIntoState,
	"_ZNK5Caver7Program13LoadIntoStateEP9lua_State",
	int, (void *this, void *L)
) {
	void **string_px = (void **)((uintptr_t)this + 0x04);
	void **string_pn = (void **)((uintptr_t)this + 0x08);
	void **bytes_px  = (void **)((uintptr_t)this + 0x0c);
	void **bytes_pn  = (void **)((uintptr_t)this + 0x10);

	LOGD("Try Load (Lawda).");

	if (*string_px != NULL) {
		char *data = (char *)**(void ***)string_px;
		if (data != NULL) {
			uint32_t len = *(uint32_t *)(data - 0xc); /* libstdc++ size */
			if (len > 0) {
				LOGI("LoadIntoState: forcing String (len=%u), ignoring Bytes", len);

				// lemme try ts
				void *saved_px = *bytes_px;
				void *saved_pn = *bytes_pn;

				*bytes_px = *string_px;
				*bytes_pn = *string_pn;

				int ok = orig_LoadIntoState(this, L);

				/* Restore */
				*bytes_px = saved_px;
				*bytes_pn = saved_pn;

				return ok;
			}
		}
	}

	/* String missing or empty → do not load any code */
	LOGI("LoadIntoState: String empty/absent – skipping load (ignoring Bytes)");
	return 0;
}

#endif /* __arm__ */

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
			LOGE("%p encountered error: %s", ps, lua_tostring(L, -1));
		}
	}
}