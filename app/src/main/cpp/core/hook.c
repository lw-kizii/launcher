#include "hook.h"
#include "log.h"
#include "../libs/Gloss.h"

#define LOG_TAG "SwordigoHooks"

static uintptr_t g_lib_bias = 0;
static void *g_engine_handle = NULL;

// hook registry
hook_installer_t g_hook_installers[HOOK_INSTALLER_CAP];
int g_hook_installer_count = 0;

// dlsym registry
dl_resolver_t g_dl_resolvers[DL_RESOLVER_CAP];
int g_dl_resolver_count = 0;

#define MOD_HOOK_CAP 256
static GHook g_mod_hooks[MOD_HOOK_CAP];
static int g_mod_hook_count = 0;
static int g_capturing_mod_hooks = 0;

typedef GHook (*GlossHookByName_fn)(const char *, const char *, void *, void **, GlossHookCallback);
typedef GHook (*GlossHook_fn)(void *, void *, void **);
typedef GHook (*GlossHookAddr_fn)(void *, void *, void **, bool, i_set);
typedef GHook (*GlossHookAddrByName_fn)(const char *, uintptr_t, void *, void **, bool, i_set, GlossHookCallback);

static GlossHookByName_fn orig_GlossHookByName = NULL;
static GlossHook_fn orig_GlossHook = NULL;
static GlossHookAddr_fn orig_GlossHookAddr = NULL;
static GlossHookAddrByName_fn orig_GlossHookAddrByName = NULL;
static int g_meta_installed = 0;

static void capture_handle(GHook h) {
	if (!h || !g_capturing_mod_hooks) return;
	if (g_mod_hook_count >= MOD_HOOK_CAP) {
		LOGE("mod hook capture cap exceeded");
		return;
	}
	g_mod_hooks[g_mod_hook_count++] = h;
}

static GHook hooked_GlossHookByName(const char *lib, const char *sym, void *nf, void **of, GlossHookCallback cb) {
	GHook h = orig_GlossHookByName(lib, sym, nf, of, cb);
	capture_handle(h);
	return h;
}

static GHook hooked_GlossHook(void *addr, void *nf, void **of) {
	GHook h = orig_GlossHook(addr, nf, of);
	capture_handle(h);
	return h;
}

static GHook hooked_GlossHookAddr(void *addr, void *nf, void **of, bool is4, i_set mode) {
	GHook h = orig_GlossHookAddr(addr, nf, of, is4, mode);
	capture_handle(h);
	return h;
}

static GHook hooked_GlossHookAddrByName(const char *lib, uintptr_t off, void *nf, void **of, bool is4, i_set mode, GlossHookCallback cb) {
	GHook h = orig_GlossHookAddrByName(lib, off, nf, of, is4, mode, cb);
	capture_handle(h);
	return h;
}

static void install_meta_hooks(void) {
	if (g_meta_installed) return;
	void *gloss = dlopen("libGlossHook.so", RTLD_NOW | RTLD_NOLOAD);
	if (!gloss) {
		LOGE("libGlossHook.so not loaded, cannot capture mod hooks");
		return;
	}
	void *pByName = dlsym(gloss, "GlossHookByName");
	void *pHook = dlsym(gloss, "GlossHook");
	void *pAddr = dlsym(gloss, "GlossHookAddr");
	void *pAddrByName = dlsym(gloss, "GlossHookAddrByName");
	if (pByName)
		GlossHook(pByName, (void *)hooked_GlossHookByName, (void **)&orig_GlossHookByName);
	if (pHook)
		GlossHook(pHook, (void *)hooked_GlossHook, (void **)&orig_GlossHook);
	if (pAddr)
		GlossHook(pAddr, (void *)hooked_GlossHookAddr, (void **)&orig_GlossHookAddr);
	if (pAddrByName)
		GlossHook(pAddrByName, (void *)hooked_GlossHookAddrByName, (void **)&orig_GlossHookAddrByName);
	g_meta_installed = 1;
	LOGI("mod-hook capture meta installed");
}

void hook_begin_mod_capture(void) {
	g_capturing_mod_hooks = 1;
}

void hook_end_mod_capture(void) {
	g_capturing_mod_hooks = 0;
}

void hook_delete_mod_hooks(void) {
	LOGI("Deleting %d mod hook(s)...", g_mod_hook_count);
	for (int i = g_mod_hook_count - 1; i >= 0; i--) {
		if (g_mod_hooks[i]) {
			GlossHookDelete(g_mod_hooks[i]);
			g_mod_hooks[i] = NULL;
		}
	}
	g_mod_hook_count = 0;
}

void hook_register_installer(hook_installer_t fn) {
	if (g_hook_installer_count >= HOOK_INSTALLER_CAP) {
		LOGE("hook_register_installer: HOOK_INSTALLER_CAP (%d) exceeded, dropping installer", HOOK_INSTALLER_CAP);
		return;
	}
	g_hook_installers[g_hook_installer_count++] = fn;
}

void dl_register_resolver(dl_resolver_t fn) {
	if (g_dl_resolver_count >= DL_RESOLVER_CAP) {
		LOGE("dl_register_resolver: DL_RESOLVER_CAP (%d) exceeded, dropping resolver", DL_RESOLVER_CAP);
		return;
	}
	g_dl_resolvers[g_dl_resolver_count++] = fn;
}

void dl_resolve_all(void) {
	LOGI("Resolving %d dlsym symbol(s)...", g_dl_resolver_count);
	for (int i = 0; i < g_dl_resolver_count; i++) {
		g_dl_resolvers[i]();
	}
}

static void ensure_bias(void) {
	if (g_lib_bias) return;
	g_lib_bias = GlossGetLibBias(HOOK_LIB_NAME);
	if (g_lib_bias) {
		LOGI("%s bias = %p", HOOK_LIB_NAME, (void*)g_lib_bias);
	} else {
		LOGE("Failed to resolve bias for %s (not loaded yet?)", HOOK_LIB_NAME);
	}
}

void *swordigo_dlsym(const char *symbol) {
	void *addr = dlsym(g_engine_handle, symbol);
	return addr;
}

uintptr_t get_lib_bias(void) {
	ensure_bias();
	return g_lib_bias;
}

uintptr_t get_lib_bss(size_t* size) {
	return GlossGetLibSection(HOOK_LIB_NAME, ".bss", size);
}

uintptr_t get_lib_data(size_t* size) {
	return GlossGetLibSection(HOOK_LIB_NAME, ".data", size);
}

uintptr_t get_lib_text(size_t* size) {
	return GlossGetLibSection(HOOK_LIB_NAME, ".text", size);
}

void init_hooks(void) {
	GlossInit(true);
	ensure_bias();

	// Sections
	size_t bss_size = 0, data_size = 0, text_size = 0;
	uintptr_t bss  = get_lib_bss(&bss_size);
	uintptr_t data = get_lib_data(&data_size);
	uintptr_t text = get_lib_text(&text_size);

	g_engine_handle = dlopen(HOOK_LIB_NAME, RTLD_NOW | RTLD_NOLOAD);

	LOGI("%s .text = %p (%zu bytes)", HOOK_LIB_NAME, (void*)text, text_size);
	LOGI("%s .data = %p (%zu bytes)", HOOK_LIB_NAME, (void*)data, data_size);
	LOGI("%s .bss = %p (%zu bytes)", HOOK_LIB_NAME, (void*)bss, bss_size);

	LOGI("Installing %d hook(s)...", g_hook_installer_count);
	for (int i = 0; i < g_hook_installer_count; i++) {
		g_hook_installers[i]();
	}

	// resolving dlsyms across everywhere! (for the launcher hooks!))
	dl_resolve_all();

	// wrap Gloss so mod .so constructors are tracked
	install_meta_hooks();
}
