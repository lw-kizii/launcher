#include "lauxlib.h"
#include "../core/hook.h"

void (*luaL_addlstring)(luaL_Buffer *, const char *, size_t);
void (*luaL_addstring)(luaL_Buffer *, const char *);
void (*luaL_addvalue)(luaL_Buffer *);
int (*luaL_argerror)(lua_State *, int, const char *);
void (*luaL_buffinit)(lua_State *, luaL_Buffer *);
int (*luaL_callmeta)(lua_State *, int, const char *);
void (*luaL_checkany)(lua_State *, int);
lua_Integer (*luaL_checkinteger)(lua_State *, int);
const char * (*luaL_checklstring)(lua_State *, int, size_t *);
lua_Number (*luaL_checknumber)(lua_State *, int);
int (*luaL_checkoption)(lua_State *, int, const char *, char const* const*);
void (*luaL_checkstack)(lua_State *, int, const char *);
void (*luaL_checktype)(lua_State *, int, int);
void * (*luaL_checkudata)(lua_State *, int, const char *);
int (*luaL_error)(lua_State *, const char *, ...);
const char * (*luaL_findtable)(lua_State *, int, const char *, int);
int (*luaL_getmetafield)(lua_State *, int, const char *);
const char * (*luaL_gsub)(lua_State *, const char *, const char *, const char *);
int (*luaL_loadbuffer)(lua_State *, const char *, size_t, const char *);
int (*luaL_loadfile)(lua_State *, const char *);
int (*luaL_loadstring)(lua_State *, const char *);
int (*luaL_newmetatable)(lua_State *, const char *);
lua_State * (*luaL_newstate)(void);
void (*luaL_openlib)(lua_State *, const char *, const luaL_Reg *, int);
lua_Integer (*luaL_optinteger)(lua_State *, int, lua_Integer);
const char * (*luaL_optlstring)(lua_State *, int, const char *, size_t *);
lua_Number (*luaL_optnumber)(lua_State *, int, lua_Number);
char * (*luaL_prepbuffer)(luaL_Buffer *);
void (*luaL_pushresult)(luaL_Buffer *);
int (*luaL_ref)(lua_State *, int);
void (*luaL_register)(lua_State *, const char *, const luaL_Reg *);
int (*luaL_typerror)(lua_State *, int, const char *);
void (*luaL_unref)(lua_State *, int, int);
void (*luaL_where)(lua_State *, int);
int (*luaopen_base)(lua_State *);
int (*luaopen_string)(lua_State *);

// Just like lua.c!
void init_lual(void) {
	luaL_addlstring = swordigo_dlsym("_Z15luaL_addlstringP11luaL_BufferPKcm");
	luaL_addstring = swordigo_dlsym("_Z14luaL_addstringP11luaL_BufferPKc");
	luaL_addvalue = swordigo_dlsym("_Z13luaL_addvalueP11luaL_Buffer");
	luaL_argerror = swordigo_dlsym("_Z13luaL_argerrorP9lua_StateiPKc");
	luaL_buffinit = swordigo_dlsym("_Z13luaL_buffinitP9lua_StateP11luaL_Buffer");
	luaL_callmeta = swordigo_dlsym("_Z13luaL_callmetaP9lua_StateiPKc");
	luaL_checkany = swordigo_dlsym("_Z13luaL_checkanyP9lua_Statei");
	luaL_checkinteger = swordigo_dlsym("_Z17luaL_checkintegerP9lua_Statei");
	luaL_checklstring = swordigo_dlsym("_Z17luaL_checklstringP9lua_StateiPm");
	luaL_checknumber = swordigo_dlsym("_Z16luaL_checknumberP9lua_Statei");
	luaL_checkoption = swordigo_dlsym("_Z16luaL_checkoptionP9lua_StateiPKcPKS2_");
	luaL_checkstack = swordigo_dlsym("_Z15luaL_checkstackP9lua_StateiPKc");
	luaL_checktype = swordigo_dlsym("_Z14luaL_checktypeP9lua_Stateii");
	luaL_checkudata = swordigo_dlsym("_Z15luaL_checkudataP9lua_StateiPKc");
	luaL_error = swordigo_dlsym("_Z10luaL_errorP9lua_StatePKcz");
	luaL_findtable = swordigo_dlsym("_Z14luaL_findtableP9lua_StateiPKci");
	luaL_getmetafield = swordigo_dlsym("_Z17luaL_getmetafieldP9lua_StateiPKc");
	luaL_gsub = swordigo_dlsym("_Z9luaL_gsubP9lua_StatePKcS2_S2_");
	luaL_loadbuffer = swordigo_dlsym("_Z15luaL_loadbufferP9lua_StatePKcmS2_");
	luaL_loadfile = swordigo_dlsym("_Z13luaL_loadfileP9lua_StatePKc");
	luaL_loadstring = swordigo_dlsym("_Z15luaL_loadstringP9lua_StatePKc");
	luaL_newmetatable = swordigo_dlsym("_Z17luaL_newmetatableP9lua_StatePKc");
	luaL_newstate = swordigo_dlsym("_Z13luaL_newstatev");
	luaL_openlib = swordigo_dlsym("_Z12luaL_openlibP9lua_StatePKcPK8luaL_Regi");
	luaL_optinteger = swordigo_dlsym("_Z15luaL_optintegerP9lua_Stateil");
	luaL_optlstring = swordigo_dlsym("_Z15luaL_optlstringP9lua_StateiPKcPm");
	luaL_optnumber = swordigo_dlsym("_Z14luaL_optnumberP9lua_Stateid");
	luaL_prepbuffer = swordigo_dlsym("_Z15luaL_prepbufferP11luaL_Buffer");
	luaL_pushresult = swordigo_dlsym("_Z15luaL_pushresultP11luaL_Buffer");
	luaL_ref = swordigo_dlsym("_Z8luaL_refP9lua_Statei");
	luaL_register = swordigo_dlsym("_Z13luaL_registerP9lua_StatePKcPK8luaL_Reg");
	luaL_typerror = swordigo_dlsym("_Z13luaL_typerrorP9lua_StateiPKc");
	luaL_unref = swordigo_dlsym("_Z10luaL_unrefP9lua_Stateii");
	luaL_where = swordigo_dlsym("_Z10luaL_whereP9lua_Statei");
	luaopen_base = swordigo_dlsym("_Z12luaopen_baseP9lua_State");
	luaopen_string = swordigo_dlsym("_Z14luaopen_stringP9lua_State");
}
