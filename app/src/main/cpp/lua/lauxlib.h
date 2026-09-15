#ifndef LAUXLIB_H
#define LAUXLIB_H

#include "lua.h"
#include <stdio.h>

#define LUA_ERRFILE (LUA_ERRERR+1)

typedef struct luaL_Reg {
	const char *name;
	lua_CFunction func;
} luaL_Reg;

#define LUA_NOREF  (-2)
#define LUA_REFNIL (-1)

#define LUAL_BUFFERSIZE BUFSIZ
typedef struct luaL_Buffer {
	char *p;
	int lvl;
	lua_State *L;
	char buffer[LUAL_BUFFERSIZE];
} luaL_Buffer;

// dlsym resolved!!!
extern void (*luaL_addlstring)(luaL_Buffer *, const char *, size_t);
extern void (*luaL_addstring)(luaL_Buffer *, const char *);
extern void (*luaL_addvalue)(luaL_Buffer *);
extern int (*luaL_argerror)(lua_State *, int, const char *);
extern void (*luaL_buffinit)(lua_State *, luaL_Buffer *);
extern int (*luaL_callmeta)(lua_State *, int, const char *);
extern void (*luaL_checkany)(lua_State *, int);
extern lua_Integer (*luaL_checkinteger)(lua_State *, int);
extern const char * (*luaL_checklstring)(lua_State *, int, size_t *);
extern lua_Number (*luaL_checknumber)(lua_State *, int);
extern int (*luaL_checkoption)(lua_State *, int, const char *, char const* const*);
extern void (*luaL_checkstack)(lua_State *, int, const char *);
extern void (*luaL_checktype)(lua_State *, int, int);
extern void * (*luaL_checkudata)(lua_State *, int, const char *);
extern int (*luaL_error)(lua_State *, const char *, ...);
extern const char * (*luaL_findtable)(lua_State *, int, const char *, int);
extern int (*luaL_getmetafield)(lua_State *, int, const char *);
extern const char * (*luaL_gsub)(lua_State *, const char *, const char *, const char *);
extern int (*luaL_loadbuffer)(lua_State *, const char *, size_t, const char *);
extern int (*luaL_loadfile)(lua_State *, const char *);
extern int (*luaL_loadstring)(lua_State *, const char *);
extern int (*luaL_newmetatable)(lua_State *, const char *);
extern lua_State * (*luaL_newstate)(void);
extern void (*luaL_openlib)(lua_State *, const char *, const luaL_Reg *, int);
extern lua_Integer (*luaL_optinteger)(lua_State *, int, lua_Integer);
extern const char * (*luaL_optlstring)(lua_State *, int, const char *, size_t *);
extern lua_Number (*luaL_optnumber)(lua_State *, int, lua_Number);
extern char * (*luaL_prepbuffer)(luaL_Buffer *);
extern void (*luaL_pushresult)(luaL_Buffer *);
extern int (*luaL_ref)(lua_State *, int);
extern void (*luaL_register)(lua_State *, const char *, const luaL_Reg *);
extern int (*luaL_typerror)(lua_State *, int, const char *);
extern void (*luaL_unref)(lua_State *, int, int);
extern void (*luaL_where)(lua_State *, int);

// these *are* in the base game
extern int (*luaopen_base)(lua_State *);
extern int (*luaopen_string)(lua_State *);

#define luaL_argcheck(L, cond, numarg, extramsg) ((void)((cond) || luaL_argerror(L, (numarg), (extramsg))))

#define luaL_checkstring(L,n) (luaL_checklstring(L, (n), NULL))
#define luaL_optstring(L,n,d) (luaL_optlstring(L, (n), (d), NULL))
#define luaL_checkint(L,n) ((int)luaL_checkinteger(L, (n)))
#define luaL_optint(L,n,d) ((int)luaL_optinteger(L, (n), (d)))
#define luaL_checklong(L,n) ((long)luaL_checkinteger(L, (n)))
#define luaL_optlong(L,n,d) ((long)luaL_optinteger(L, (n), (d)))

#define luaL_typename(L,i) lua_typename(L, lua_type(L,(i)))

#define luaL_dofile(L, fn) (luaL_loadfile(L, fn) || lua_pcall(L, 0, LUA_MULTRET, 0))
#define luaL_dostring(L, s) (luaL_loadstring(L, s) || lua_pcall(L, 0, LUA_MULTRET, 0))
#define luaL_getmetatable(L,n) (lua_getfield(L, LUA_REGISTRYINDEX, (n)))
#define luaL_opt(L,f,n,d) (lua_isnoneornil(L,(n)) ? (d) : f(L,(n)))

#define luaL_getn(L,i) ((int)lua_objlen(L, (i)))
#define luaL_setn(L,i,j) ((void)0)

#define LUA_QL(x) "'" x "'"
#define LUA_QS LUA_QL("%s")

void init_lual(void);

#endif
