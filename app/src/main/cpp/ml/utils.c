#include "ml.h"
#include "java.h"
#include "log.h"
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>

#define LOG_TAG "LauncherMLUtils"

const char *ML_path_basename(const char *path) {
	const char *slash = strrchr(path, '/');
	return slash ? slash + 1 : path;
}

void ML_ensure_dir(const char *path) {
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

// Order: .../net.kiwi.launcher/files/:
// 1. /resources/
// 2. /mods/<mod>/resources/
// 3. AssetManager (Vanilla Assets)
FILE *ML_fetch_asset(String *asset) {
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

int ML_is_save_ext(const char *ext) {
	return ext && !strcmp(ext, "gplayer");
}

int ML_is_save_path(const char *p) {
	return p && strstr(p, ".gplayer");
}

void ML_redirect_path(String *out, const char *orig) {
	const char *id = java_current_mod_id();
	if (!id || !*id) {
		String_create(out, orig);
		return;
	}
	const char *base = ML_path_basename(orig);
	char full[512];
	snprintf(full, sizeof(full), "%s%s", java_resource_path("saves/"), base);
	String_create(out, full);
}
