#include "lua.h"
#include "hook.h"
#include "lauxlib.h"
#include "ProgramState.h"
#include "log.h"
#include "overlay/main.h"
#include "lua_trace.h"

// lua libs
#include "lualib.h"

#define LOG_TAG "ProgramState"

extern void API_register_mini(lua_State *L);
extern void API_register_io(lua_State *L);
extern void API_register_fs(lua_State *L);
extern void API_register_os(lua_State *L);
extern void API_register_shader(lua_State *L);

static void lua_call_hook(lua_State *L, lua_Debug *ar) {
	if (!ar || ar->event != LUA_HOOKCALL) return;
	lua_getinfo(L, "nS", ar);
	const char *name = ar->name;
	if (!name || !name[0]) {
		if (ar->what && ar->what[0] == 'm') name = "(main)";
		else if (ar->what && ar->what[0] == 'C') name = "(C)";
		else name = "?";
	}
	lua_trace_record(name);
}

HOOK_SYMBOL(
	RegisterProgramLibrary,
	"_ZN5Caver12ProgramState22RegisterProgramLibraryEv",
	void, (ProgramState *this)
) {
	orig_RegisterProgramLibrary(this);
	lua_State *L = this->L;

	// Lua libs first.
	luaopen_table(L);
	luaopen_math(L);
	luaopen_debug(L);

	API_register_java_stuff(L);
	API_register_mini(L);
	API_register_io(L);
	API_register_fs(L);
	API_register_os(L);
	API_register_shader(L);

	if (lua_sethook && lua_getinfo)
		lua_sethook(L, lua_call_hook, LUA_MASKCALL, 0);

	LOGD("Lua libraries registered.");
}

void init_API() {
	initAPI_java();
}
