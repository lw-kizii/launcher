#ifndef LAUNCHER_HOOK_H
#define LAUNCHER_HOOK_H

#include "../libs/Gloss.h"
#include <dlfcn.h>
#include <stdint.h>
#include <stddef.h>

// libswordigo.so
#define HOOK_LIB_NAME "libswordigo.so"

// archSplit
// Return something based on the architecture type!
#if defined(__aarch64__)
#define archSplit(arm_val, arm64_val) (arm64_val)
#elif defined(__arm__)
#define archSplit(arm_val, arm64_val) (arm_val)
#else
#error "GlossHook only supports arm/arm64"
#endif

// Don't know why this is needed when archSplit exists but a
#define OFFSET(off32, off64) ((uintptr_t)archSplit(off32, off64))
#define $(type, base, off32, off64) \
	((type *)((uintptr_t)(base) + OFFSET(off32, off64)))

// Static hook symbol
#define HOOK_SYMBOL(name, symbol_str, ret, args) \
	typedef ret (*name##_t) args; \
	static name##_t orig_##name = NULL; \
	static ret hook_##name args; \
	__attribute__((constructor)) \
	static void register_##name(void) { \
		GlossInit(true); \
		GlossHookByName( \
			HOOK_LIB_NAME, \
			symbol_str, \
			(void *)hook_##name, \
			(void **)&orig_##name, \
			NULL \
		); \
	} \
	static ret hook_##name args

// Offset hooks. Need to be registered!
#define HOOK_OFFSET(name, off32, off64, ret, args) \
	typedef ret (*name##_t) args; \
	static name##_t orig_##name = NULL; \
	static ret hook_##name args; \
	static void register_##name(void) { \
		GlossInit(true); \
		GlossHookAddrByName( \
			HOOK_LIB_NAME, \
			OFFSET(off32, off64), \
			(void *)hook_##name, \
			(void **)&orig_##name, \
			false, \
			archSplit(I_THUMB, I_ARM64), \
			NULL \
		); \
	} \
	static ret hook_##name args

// init_hooks calls this.
typedef void (*dl_resolver_t)(void);

#define DL_RESOLVER_CAP 512

extern dl_resolver_t g_dl_resolvers[DL_RESOLVER_CAP];
extern int g_dl_resolver_count;

void dl_register_resolver(dl_resolver_t fn);
void dl_resolve_all(void);

// Static DL_SYMBOL
#define DL_SYMBOL(name, symbol_str, ret, args) \
	typedef ret (*name##_t) args; \
	static name##_t name = NULL; \
	static void resolve_##name(void) { \
		if (name) return; \
		void *h = dlopen(HOOK_LIB_NAME, RTLD_NOW | RTLD_NOLOAD); \
		if (h) name = (name##_t)dlsym(h, symbol_str); \
	} \
	__attribute__((constructor)) \
	static void register_##name(void) { \
		dl_register_resolver(resolve_##name); \
	} \
	struct __dl_symbol_semicolon_##name { int _unused; }

// Header declaration for G_DL_SYMBOL, used for caver functions `hooks/`
#define DL_SYMBOL_DECL(name, ret, args) \
	extern ret (*name) args; \
	void resolve_##name(void)

// DL_SYMBOL but can be exposed globally
#define G_DL_SYMBOL(name, symbol_str, ret, args) \
	typedef ret (*name##_t) args; \
	name##_t name = NULL; \
	void resolve_##name(void) { \
		if (name) return; \
		void *h = dlopen(HOOK_LIB_NAME, RTLD_NOW | RTLD_NOLOAD); \
		if (h) name = (name##_t)dlsym(h, symbol_str); \
	} \
	__attribute__((constructor)) \
	static void register_##name(void) { \
		dl_register_resolver(resolve_##name); \
	}

uintptr_t get_lib_bias(void);
uintptr_t get_lib_bss(size_t *size);
uintptr_t get_lib_data(size_t *size);
uintptr_t get_lib_text(size_t *size);

// dlsym from libswordigo...
void *swordigo_dlsym(const char *symbol);


void init_hooks(void);

void load_mod_libraries(void);
void unload_mod_libraries(void);

#endif
