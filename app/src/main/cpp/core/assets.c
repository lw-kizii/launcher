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
#include <fcntl.h>
#include <zlib.h>
#include <setjmp.h>
#include <signal.h>

#define LOG_TAG "LauncherAssets"

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

// A function used to fetch assets!
// Order: .../net.kiwi.launcher/files/:
// 1. /resources/
// 2. /mods/<mod>/resources/
// 3. AssetManager (Vanilla Assets)
FILE *fetch_asset(String *asset) {
	const char *name = String_get(asset);
	if (!name || !name[0]) return NULL;

	const char *ext = java_external_files();
	if (ext && ext[0]) {
		char global[512];
		snprintf(global, sizeof(global), "%s/%s", ext, name);
		FILE *f = fopen(global, "rb");
		if (f) return f;
	}

	const char *modpath = java_resource_path(name);
	FILE *f = fopen(modpath, "rb");
	if (f) return f;

	return NULL;
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
	/* scl, scene, pvr and pod */
	FILE *f = fetch_asset(file);
	if (!f) goto bailout;

	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (size < 0) goto bailout;
	void *buf = malloc((size_t)size);
	if (!buf) goto bailout;
	size_t readbytes = fread(buf, 1, (size_t)size, f);
	fclose(f);
	*param_2 = (uint)readbytes;
	return buf;

	bailout:
	if (f) fclose(f);
	return orig_NewByteBufferFromAA(file, param_2);
}

static bool file_exists(const char *path) {
	struct stat st;
	return path && path[0] && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

HOOK_SYMBOL(
	BinaryFile_Open,
	"_ZN5Caver10BinaryFile4OpenERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEENS0_4ModeEb",
	uint, (void *this, String *filename, int mode, bool use_asset)
) {
	FILE *f = fetch_asset(filename);
	if (!f) return orig_BinaryFile_Open(this, filename, mode, use_asset);
	int fd = dup(fileno(f)); // Don't kill the gzdopen!
	fclose(f);
	if (fd < 0) return 0;
	const char *m = (mode == 1) ? "wb" : "rb";
	void *gz = gzdopen(fd, m);
	if (!gz) { close(fd); return 0; }
	*(int *)this = 2;
	*(void **)(this + 8) = gz;
	*(int *)(this + 0x10) = 0;
	return 1;
}

HOOK_SYMBOL(
	GetAudioFileData,
	"_ZN5Caver16GetAudioFileDataERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEEPNS_11AudioBuffer12BufferFormatEPPvPiSE_",
	bool, (String *path, int *fmt, void **data, int *size, int *rate)
) {
	FILE *f = fetch_asset(path);
	if (!f) return orig_GetAudioFileData(path, fmt, data, size, rate);
	unsigned char hdr[0x2c]; // WAV header... 0x2c
	if (fread(hdr, 1, 0x2c, f) != 0x2c) {
		fclose(f);
		return false;
	}

	/*
	 bVar2 = (((iVar3 == 0x2c && local_94[0] == 0x46464952) && local_8c == 0x45564157) &&
	 iStack_88 == 0x20746d66) && local_70 == 0x61746164;
	 if ((((iVar3 == 0x2c && local_94[0] == 0x46464952) && local_8c == 0x45564157) &&
	 iStack_88 == 0x20746d66) && local_70 == 0x61746164) {
	 */
	if (*(uint *)(hdr + 0) != 0x46464952 || *(uint *)(hdr + 8) != 0x45564157 || *(uint *)(hdr + 12) != 0x20746d66 || *(uint *)(hdr + 36) != 0x61746164) {
		fclose(f);
		return false;
	}

	uint datasz = *(uint *)(hdr + 40); // wav + 0x40 == sizeof raw PCM!! (Subchunk2Size)
	void *buf = malloc(datasz);

	// Edge case.
	// No memory or the wav file was shorter
	if (!buf || fread(buf, 1, datasz, f) != datasz) {
		free(buf); // free our buffer!
		fclose(f); // ...and close the file too.
		return false; // no valid data.
	}
	fclose(f);

	short ch = *(short *)(hdr + 22); // 22: NumChannels | Mono = 1, Stereo = 2, etc.
	short bits = *(short *)(hdr + 34); // 34: BitsPerSample | 8 bits = 8, 16 bits = 16, etc.
	int sr = *(int *)(hdr + 24); // 24: SampleRate | 8000, 44100, etc.

	int bf = 0; // default to an invalid format!!
	if (bits == 16) bf = (ch == 1) ? 2 : 4;
	if (bits == 8) bf = (ch == 1) ? 1 : 3;
	// 1 = 8 bit mono, 2 = 16 bit mono
	// 3 = 8 bit stereo, 4 = 16 bit stereo

	*fmt = bf; // BufferFormat that the audio system expects
	*data = buf; // Pointer to Audio Data
	*size = (int)datasz; // Size of Audio Data
	*rate = sr; // Sample Rate for OpenAL

	return true; // success ^^
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
//	LOGD("redirect %s -> %s", orig, full);
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
		FILE *f = fetch_asset(path);
		if (f) { fclose(f); return true; }
	}
	return orig_FileExistsAtPath(path);
}

// Library loading logic!!!

// This is way better!
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

void load_mod_libraries(void) {
	unload_mod_libraries();

	const char *id = java_current_mod_id();
	if (!id || !id[0]) return;

	const char *ext = java_external_files();
	if (!ext || !ext[0]) {
		LOGE("load_mod_libraries: external files path missing");
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
		LOGI("load_mod_libraries: no libraries dir for mod '%s'", id);
		return;
	}
	LOGI("load_mod_libraries: using %s", libdir);

	hook_begin_mod_capture();
	struct dirent *ent;
	while ((ent = readdir(d)) != NULL) {
		if (ent->d_name[0] == '.' || !has_so_ext(ent->d_name)) continue;

		if (g_mod_handle_count >= MOD_LIB_CAP) {
			LOGE("load_mod_libraries: handle cap reached, skipping %s", ent->d_name);
			continue;
		}

		void *h = loadlib(libdir, ent->d_name);
		if (!h) continue;

		typedef void (*mod_init_fn)(void);
		mod_init_fn init = (mod_init_fn)dlsym(h, "mod_init");
		if (init) init();

		g_mod_handles[g_mod_handle_count++] = h;
		LOGI("load_mod_libraries: loaded %s", ent->d_name);
	}
	closedir(d);
	hook_end_mod_capture();

	LOGI("load_mod_libraries: %d library(ies) for mod '%s'", g_mod_handle_count, id);
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

void unload_mod_libraries(void) {
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
