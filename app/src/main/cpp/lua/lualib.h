// lauxlib.
#ifndef LUALIB_H
#define LUALIB_H

#include "lua.h"

#define LUA_FILEHANDLE "FILE*"

#define LUA_COLIBNAME "coroutine"
#define LUA_TABLIBNAME "table"
#define LUA_IOLIBNAME "io"
#define LUA_OSLIBNAME "os"
#define LUA_STRLIBNAME "string"
#define LUA_MATHLIBNAME "math"
#define LUA_DBLIBNAME "debug"
#define LUA_LOADLIBNAME "package"

// refer to lauxlib.c too.
/* table/math/debug openers. io/os have custom implementation ^^ */

int luaopen_table(lua_State *L);
int luaopen_math(lua_State *L);
int luaopen_debug(lua_State *L);

#define lua_assert(x) ((void)0)

#endif
