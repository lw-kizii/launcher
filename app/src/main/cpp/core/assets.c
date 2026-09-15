#include "hook.h"
#include "java.h"
#include "core.h"
#include "log.h"
#include "stdstring.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <jni.h>
#include <dlfcn.h>

#define LOG_TAG "LauncherAssets"

// This is way better!
#define MOD_LIB_CAP 16
static void *g_mod_handles[MOD_LIB_CAP];
static int g_mod_handle_count = 0;

static const char *path_basename(const char *path) {
	const char *slash = strrchr(path, '/');
	return slash ? slash + 1 : path;
}

static void ensure_dir(const char *path) {
	char buf[512];
	snprintf(buf, sizeof(buf), "%s", path);
	for (char *p = buf + 1; *p; p++) {
		if (*p == '/') {
			*p = '\0';
			mkdir(buf, 0770);
			*p = '/';
		}
	}
	mkdir(buf, 0770);
}

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

HOOK_SYMBOL(
	NewByteBufferFromAA,
	"_ZN5Caver29NewByteBufferFromAndroidAssetERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEEPj",
	void*, (String *file, uint *param_2)
	) {
	const char *respath = java_resource_path(String_get(file));
	FILE *f = fopen(respath, "rb");
	if (!f) goto bailout;

	// This function *does not* handle backgrounds/wav files

//	fclose(f);
//	String s;
//	String_create(&s, respath);
//	return orig_NewByteBufferFromAA(&s, param_2);
	fseek(f, 0, SEEK_END);
	size_t size = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (size < 0) goto bailout;
	void *buf = malloc(size);
	if (!buf) goto bailout;
	size_t readbytes = fread(buf, 1, size, f);
	fclose(f); // ty kizi for the warning
	*param_2 = (uint)readbytes; // param2 is outbytes
	LOGD("Modded Asset %s", respath);
	return buf;

	bailout:
	if (f) fclose(f);
	return orig_NewByteBufferFromAA(file, param_2);
}

static int is_save_path(const char *p) {
	return p && strstr(p, ".gplayer");
}

static void redirect_path(String *out, const char *orig) {
	const char *id = java_current_mod_id();
	if (!id || !*id) {
		String_create(out, orig);
		return;
	}
	const char *base = path_basename(orig);
	char full[512];
	snprintf(full, sizeof(full), "%s%s", java_resource_path("saves/"), base);
	LOGD("redirect %s -> %s", orig, full);
	String_create(out, full);
}

HOOK_SYMBOL(
	FileExistsAtPath,
	"_ZN5Caver16FileExistsAtPathERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE",
	bool, (String *path)
) {
	const char *p = String_get(path);
	if (!p || !p[0]) return orig_FileExistsAtPath(path);

	if (is_save_path(p)) {
		String s;
		redirect_path(&s, p);
		bool ret = orig_FileExistsAtPath(&s);
		String_destroy(&s);
		return ret;
	}

	if (p[0] != '/') {
		const char *id = java_current_mod_id();
		if (id && id[0]) {
			const char *respath = java_resource_path(p);
			struct stat st;
			if (respath && respath[0] && stat(respath, &st) == 0 && S_ISREG(st.st_mode)) return true;
		}
	}

	return orig_FileExistsAtPath(path);
}


void load_mod_libraries(void) {
	unload_mod_libraries(); // unload previous libraries if any!

	const char *id = java_current_mod_id();
	if (!id || !id[0]) return; // just in case...

	const char *ext = java_external_files();
	if (!ext || !ext[0]) {
		LOGE("load_mod_libraries: external files path missing");
		return;
	}

	char libdir[512];
	snprintf(libdir, sizeof(libdir), "%s/mods/%s/libraries", ext, id);

	DIR *d = opendir(libdir);
	if (!d) {
		LOGI("load_mod_libraries: no libraries dir for mod '%s'", id);
		return;
	}

	struct dirent *ent;
	while ((ent = readdir(d)) != NULL) {
		const char *name = ent->d_name;
		size_t len = strlen(name);
		if (len < 4 || strcmp(name + len - 3, ".so") != 0) continue;
		if (name[0] == '.') continue;

		char path[768];
		snprintf(path, sizeof(path), "%s/%s", libdir, name);

		void *h = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
		if (!h) {
			LOGE("load_mod_libraries: dlopen failed %s: %s", path, dlerror());
			continue;
		}

		if (g_mod_handle_count >= MOD_LIB_CAP) {
			LOGE("load_mod_libraries: handle cap reached, skipping %s", name);
			dlclose(h);
			continue;
		}

		g_mod_handles[g_mod_handle_count++] = h;
		LOGI("load_mod_libraries: loaded %s", path);
	}
	closedir(d);

	LOGI("load_mod_libraries: %d library(ies) for mod '%s'", g_mod_handle_count, id);
}

void unload_mod_libraries(void) {
	for (int i = g_mod_handle_count - 1; i >= 0; i--) {
		if (g_mod_handles[i]) {
			dlclose(g_mod_handles[i]);
			g_mod_handles[i] = NULL;
		}
	}
	g_mod_handle_count = 0;
}

//extern void save_manager_on_mod_exit(void);
/* called from Java when we leave a mod / return to launcher */
void assets_on_mod_exit(void) {
	unload_mod_libraries();
	java_reset_mod_id();
//	save_manager_on_mod_exit();
	LOGI("assets_on_mod_exit: state cleared");
}

void init_assets(void) {
	LOGI("Start of Assets Override");
}
