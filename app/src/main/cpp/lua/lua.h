//* refer to lua.c to see how the symbols are dynamically linked! *//
#ifndef LUA_H
#define LUA_H

#include <stdarg.h>
#include <stddef.h>

#define LUA_VERSION "Lua 5.1"
#define LUA_RELEASE "Lua 5.1.5"
#define LUA_VERSION_NUM 501

#define LUA_REGISTRYINDEX (-10000)
#define LUA_ENVIRONINDEX  (-10001)
#define LUA_GLOBALSINDEX  (-10002)
#define lua_upvalueindex(i) (LUA_GLOBALSINDEX - (i))

#define LUA_YIELD     1
#define LUA_ERRRUN    2
#define LUA_ERRSYNTAX 3
#define LUA_ERRMEM    4
#define LUA_ERRERR    5

#define LUA_MULTRET (-1)

typedef struct lua_State lua_State;
typedef int (*lua_CFunction)(lua_State *L);
typedef const char *(*lua_Reader)(lua_State *L, void *ud, size_t *sz);
typedef int (*lua_Writer)(lua_State *L, const void *p, size_t sz, void *ud);
typedef void *(*lua_Alloc)(void *ud, void *ptr, size_t osize, size_t nsize);

#define LUA_TNONE          (-1)
#define LUA_TNIL            0
#define LUA_TBOOLEAN        1
#define LUA_TLIGHTUSERDATA  2
#define LUA_TNUMBER         3
#define LUA_TSTRING         4
#define LUA_TTABLE          5
#define LUA_TFUNCTION       6
#define LUA_TUSERDATA       7
#define LUA_TTHREAD         8

#define LUA_MINSTACK 20

typedef double lua_Number;
typedef ptrdiff_t lua_Integer;

#define LUA_IDSIZE 60
typedef struct lua_Debug {
	int event;
	const char *name;
	const char *namewhat;
	const char *what;
	const char *source;
	int currentline;
	int nups;
	int linedefined;
	int lastlinedefined;
	char short_src[LUA_IDSIZE];
	int i_ci;
} lua_Debug;

typedef void (*lua_Hook)(lua_State *L, lua_Debug *ar);

#define LUA_HOOKCALL    0
#define LUA_HOOKRET     1
#define LUA_HOOKLINE    2
#define LUA_HOOKCOUNT   3
#define LUA_HOOKTAILRET 4

#define LUA_MASKCALL  (1 << LUA_HOOKCALL)
#define LUA_MASKRET   (1 << LUA_HOOKRET)
#define LUA_MASKLINE  (1 << LUA_HOOKLINE)
#define LUA_MASKCOUNT (1 << LUA_HOOKCOUNT)

#define LUA_GCSTOP       0
#define LUA_GCRESTART    1
#define LUA_GCCOLLECT    2
#define LUA_GCCOUNT      3
#define LUA_GCCOUNTB     4
#define LUA_GCSTEP       5
#define LUA_GCSETPAUSE   6
#define LUA_GCSETSTEPMUL 7

/* function pointers resolved by init_lua() */
extern lua_CFunction (*lua_atpanic)(lua_State *, lua_CFunction);
extern void (*lua_call)(lua_State *, int, int);
extern int (*lua_checkstack)(lua_State *, int);
extern void (*lua_close)(lua_State *);
extern void (*lua_concat)(lua_State *, int);
extern int (*lua_cpcall)(lua_State *, lua_CFunction, void *);
extern void (*lua_createtable)(lua_State *, int, int);
extern int (*lua_dump)(lua_State *, lua_CFunction, void *);
extern int (*lua_equal)(lua_State *, int, int);
extern int (*lua_error)(lua_State *);
extern int (*lua_gc)(lua_State *, int, int);
extern lua_Alloc (*lua_getallocf)(lua_State *, void **);
extern void (*lua_getfenv)(lua_State *, int);
extern void (*lua_getfield)(lua_State *, int, const char *);
extern lua_Hook (*lua_gethook)(lua_State *);
extern int (*lua_gethookcount)(lua_State *);
extern int (*lua_gethookmask)(lua_State *);
extern int (*lua_getinfo)(lua_State *, const char *, lua_Debug *);
extern const char * (*lua_getlocal)(lua_State *, const lua_Debug *, int);
extern int (*lua_getmetatable)(lua_State *, int);
extern int (*lua_getstack)(lua_State *, int, lua_Debug *);
extern void (*lua_gettable)(lua_State *, int);
extern int (*lua_gettop)(lua_State *);
extern const char * (*lua_getupvalue)(lua_State *, int, int);
extern void (*lua_insert)(lua_State *, int);
extern int (*lua_iscfunction)(lua_State *, int);
extern int (*lua_isnumber)(lua_State *, int);
extern int (*lua_isstring)(lua_State *, int);
extern int (*lua_isuserdata)(lua_State *, int);
extern int (*lua_lessthan)(lua_State *, int, int);
extern int (*lua_load)(lua_State *, lua_Reader, void *, const char *);
extern lua_State * (*lua_newstate)(lua_Alloc, void *);
extern lua_State * (*lua_newthread)(lua_State *);
extern void * (*lua_newuserdata)(lua_State *, size_t);
extern int (*lua_next)(lua_State *, int);
extern size_t (*lua_objlen)(lua_State *, int);
extern int (*lua_pcall)(lua_State *, int, int, int);
extern void (*lua_pushboolean)(lua_State *, int);
extern void (*lua_pushcclosure)(lua_State *, lua_CFunction, int);
extern const char * (*lua_pushfstring)(lua_State *, const char *, ...);
extern void (*lua_pushinteger)(lua_State *, lua_Integer);
extern void (*lua_pushlightuserdata)(lua_State *, void *);
extern void (*lua_pushlstring)(lua_State *, const char *, size_t);
extern void (*lua_pushnil)(lua_State *);
extern void (*lua_pushnumber)(lua_State *, lua_Number);
extern void (*lua_pushstring)(lua_State *, const char *);
extern int (*lua_pushthread)(lua_State *);
extern void (*lua_pushvalue)(lua_State *, int);
extern const char * (*lua_pushvfstring)(lua_State *, const char *, va_list);
extern int (*lua_rawequal)(lua_State *, int, int);
extern void (*lua_rawget)(lua_State *, int);
extern void (*lua_rawgeti)(lua_State *, int, int);
extern void (*lua_rawset)(lua_State *, int);
extern void (*lua_rawseti)(lua_State *, int, int);
extern void (*lua_remove)(lua_State *, int);
extern void (*lua_replace)(lua_State *, int);
extern int (*lua_resume)(lua_State *, int);
extern void (*lua_setallocf)(lua_State *, lua_Alloc, void *);
extern int (*lua_setfenv)(lua_State *, int);
extern void (*lua_setfield)(lua_State *, int, const char *);
extern int (*lua_sethook)(lua_State *, lua_Hook, int, int);
extern void (*lua_setlevel)(lua_State *, lua_State *);
extern const char * (*lua_setlocal)(lua_State *, const lua_Debug *, int);
extern int (*lua_setmetatable)(lua_State *, int);
extern void (*lua_settable)(lua_State *, int);
extern void (*lua_settop)(lua_State *, int);
extern const char * (*lua_setupvalue)(lua_State *, int, int);
extern int (*lua_status)(lua_State *);
extern int (*lua_toboolean)(lua_State *, int);
extern lua_CFunction (*lua_tocfunction)(lua_State *, int);
extern lua_Integer (*lua_tointeger)(lua_State *, int);
extern const char * (*lua_tolstring)(lua_State *, int, size_t *);
extern lua_Number (*lua_tonumber)(lua_State *, int);
extern const void * (*lua_topointer)(lua_State *, int);
extern lua_State * (*lua_tothread)(lua_State *, int);
extern void * (*lua_touserdata)(lua_State *, int);
extern int (*lua_type)(lua_State *, int);
extern const char * (*lua_typename)(lua_State *, int);
extern void (*lua_xmove)(lua_State *, lua_State *, int);
extern int (*lua_yield)(lua_State *, int);

#define lua_pop(L,n)            lua_settop(L, -(n)-1)
#define lua_newtable(L)         lua_createtable(L, 0, 0)
#define lua_pushcfunction(L,f)  lua_pushcclosure(L, (f), 0)
#define lua_strlen(L,i)         lua_objlen(L, (i))
#define lua_isfunction(L,n)     (lua_type(L, (n)) == LUA_TFUNCTION)
#define lua_istable(L,n)        (lua_type(L, (n)) == LUA_TTABLE)
#define lua_islightuserdata(L,n)(lua_type(L, (n)) == LUA_TLIGHTUSERDATA)
#define lua_isnil(L,n)          (lua_type(L, (n)) == LUA_TNIL)
#define lua_isboolean(L,n)      (lua_type(L, (n)) == LUA_TBOOLEAN)
#define lua_isthread(L,n)       (lua_type(L, (n)) == LUA_TTHREAD)
#define lua_isnone(L,n)         (lua_type(L, (n)) == LUA_TNONE)
#define lua_isnoneornil(L,n)    (lua_type(L, (n)) <= 0)
#define lua_pushliteral(L, s)   lua_pushlstring(L, "" s, (sizeof(s)/sizeof(char))-1)
#define lua_setglobal(L,s)      lua_setfield(L, LUA_GLOBALSINDEX, (s))
#define lua_getglobal(L,s)      lua_getfield(L, LUA_GLOBALSINDEX, (s))
#define lua_tostring(L,i)       lua_tolstring(L, (i), NULL)

void init_lua(void);

#endif
