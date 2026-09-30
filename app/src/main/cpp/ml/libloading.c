#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <dlfcn.h>
#include "java.h"
#include "hook.h"
#include "log.h"
#include "ml.h"

#define LOG_TAG "LibraryLoader"

static int copy_file(const char *src, const char *dst) {
	FILE *in = fopen(src, "rb");
	if (!in) return 0;

	FILE *out = fopen(dst, "wb");
	if (!out) {
		fclose(in);
		return 0;
	}

	char buf[8192];
	size_t n;
	int ok = 1;
	while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
		if (fwrite(buf, 1, n, out) != n) {
			ok = 0;
			break;
		}
	}

	fclose(in);
	fclose(out);
	if (!ok) remove(dst);
	return ok;
}

#define MOD_LIB_CAP 16
static void *g_mod_handles[MOD_LIB_CAP];
static char g_mod_lib_paths[MOD_LIB_CAP][1024];
static int g_mod_handle_count = 0;
typedef void (*unload_mod_fn)(void);

static int has_so_ext(const char *name) {
	size_t len = strlen(name);
	return len >= 4 && strcmp(name + len - 3, ".so") == 0;
}

static void *loadlib(const char *libdir, const char *name) {
	const char *cachedir = java_internal_cache();

	// String buffer...
	char src[1024], dst[1024];
	snprintf(src, sizeof(src), "%s/%s", libdir, name);
	snprintf(dst, sizeof(dst), "%s/%s", cachedir, name);

	if (!copy_file(src, dst)) return NULL;

	void *h = dlopen(dst, RTLD_NOW | RTLD_GLOBAL);
	if (!h) {
		LOGE("loadlib: dlopen failed %s: %s", dst, dlerror());
		remove(dst);
		return NULL;
	}
	snprintf(g_mod_lib_paths[g_mod_handle_count], sizeof(g_mod_lib_paths[0]), "%s", dst);
	return h;
}

static void clear_cache_sos(void) {
	const char *cachedir = java_internal_cache();
	if (!cachedir || !cachedir[0]) return;
	DIR *d = opendir(cachedir);
	if (!d) return;
	struct dirent *ent;
	while ((ent = readdir(d)) != NULL) {
		if (ent->d_name[0] == '.' || !has_so_ext(ent->d_name)) continue;
		char path[1024];
		snprintf(path, sizeof(path), "%s/%s", cachedir, ent->d_name);
		remove(path);
		LOGI("cleared cache so: %s", ent->d_name);
	}
	closedir(d);
}

void ML_unload_mod_libraries(void) {
	hook_delete_mod_hooks();
	for (int i = g_mod_handle_count - 1; i >= 0; i--) {
		if (g_mod_handles[i]) {
			dlclose(g_mod_handles[i]);
			g_mod_handles[i] = NULL;
		}
		if (g_mod_lib_paths[i][0]) {
			remove(g_mod_lib_paths[i]);
			g_mod_lib_paths[i][0] = '\0';
		}
	}
	g_mod_handle_count = 0;
	clear_cache_sos();
}

void ML_load_mod_libraries(void) {
	ML_unload_mod_libraries();

	const char *id = java_current_mod_id();
	if (!id || !id[0]) return;

	const char *ext = java_external_files();
	if (!ext || !ext[0]) {
		LOGE("ML_load_mod_libraries: external files path missing");
		return;
	}

	char libdir[512];
	const char *abi = archSplit("armeabi-v7a", "arm64-v8a");
	snprintf(libdir, sizeof(libdir), "%s/mods/%s/libraries/%s", ext, id, abi);
	DIR *d = opendir(libdir);
	if (!d) {
		snprintf(libdir, sizeof(libdir), "%s/mods/%s/libraries", ext, id);
		d = opendir(libdir);
	}
	if (!d) {
		LOGI("ML_load_mod_libraries: no libraries dir for mod '%s'", id);
		return;
	}
	LOGI("ML_load_mod_libraries: using %s", libdir);

	hook_begin_mod_capture();
	struct dirent *ent;
	while ((ent = readdir(d)) != NULL) {
		if (ent->d_name[0] == '.' || !has_so_ext(ent->d_name)) continue;

		if (g_mod_handle_count >= MOD_LIB_CAP) {
			LOGE("ML_load_mod_libraries: handle cap reached, skipping %s", ent->d_name);
			continue;
		}

		void *h = loadlib(libdir, ent->d_name);
		if (!h) continue;

		typedef void (*mod_init_fn)(void);
		mod_init_fn init = (mod_init_fn)dlsym(h, "mod_init");
		if (init) init();

		g_mod_handles[g_mod_handle_count++] = h;
		LOGI("ML_load_mod_libraries: loaded %s", ent->d_name);
	}
	closedir(d);
	hook_end_mod_capture();

	LOGI("ML_load_mod_libraries: %d library(ies) for mod '%s'", g_mod_handle_count, id);
}
