#include "hook.h"
#include "ml.h"
#include "java.h"
#include "log.h"
#include <stdio.h>

#define LOG_TAG "LauncherMLSavesHooks"

HOOK_SYMBOL(
	GetFilesWithExtension,
	"_ZN5Caver21GetFilesWithExtensionERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEES8_PNS0_6vectorIS6_NS4_IS6_EEEE",
	void, (String *extension, String *path, void *outfiles)
) {
	const char *ext = String_get(extension);
	const char *p = String_get(path);
	LOGD("GetFilesWithExtension ext=%s path=%s", ext, p);
	const char *id = java_current_mod_id();
	if (id && *id && ML_is_save_ext(ext)) {
		const char *savesdir = java_resource_path("saves/");
		ML_ensure_dir(savesdir);
		String modpath;
		String_create(&modpath, savesdir);
		orig_GetFilesWithExtension(extension, &modpath, outfiles);
		String_destroy(&modpath);
		return;
	}
	orig_GetFilesWithExtension(extension, path, outfiles);
}

HOOK_SYMBOL(
	NewByteBufferFromFile,
	"_ZN5Caver21NewByteBufferFromFileERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEEPj",
	void*, (String *path, uint *out_size)
) {
	const char *p = String_get(path);
	LOGD("NewByteBufferFromFile %s", p);
	if (!ML_is_save_path(p)) return orig_NewByteBufferFromFile(path, out_size);
	String s;
	ML_redirect_path(&s, p);
	void *ret = orig_NewByteBufferFromFile(&s, out_size);
	String_destroy(&s);
	return ret;
}

HOOK_SYMBOL(
	SaveByteBufferToFile,
	"_ZN5Caver20SaveByteBufferToFileEPKhjRKNSt6__ndk112basic_stringIcNS2_11char_traitsIcEENS2_9allocatorIcEEEE",
	uint, (const unsigned char *buf, uint size, String *path)
) {
	const char *p = String_get(path);
	LOGD("SaveByteBufferToFile %s size=%u", p, size);
	if (!ML_is_save_path(p)) return orig_SaveByteBufferToFile(buf, size, path);

	const char *id = java_current_mod_id();
	if (!id || !*id) return orig_SaveByteBufferToFile(buf, size, path);

	const char *base = ML_path_basename(p);
	char full[512];
	snprintf(full, sizeof(full), "%s%s", java_resource_path("saves/"), base);
	ML_ensure_dir(java_resource_path("saves/"));
	LOGD("writing ourselves to %s", full);

	FILE *f = fopen(full, "wb");
	if (!f) {
		LOGE("fopen failed for %s", full);
		return 0;
	}
	size_t written = fwrite(buf, 1, size, f);
	fclose(f);
	LOGD("wrote %zu bytes", written);
	return written == size ? 1 : 0;
}

HOOK_SYMBOL(
	DeleteFileAtPath,
	"_ZN5Caver16DeleteFileAtPathERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE",
	bool, (String *path)
) {
	const char *p = String_get(path);
	LOGD("DeleteFileAtPath %s", p);
	if (!ML_is_save_path(p)) return orig_DeleteFileAtPath(path);
	String s;
	ML_redirect_path(&s, p);
	bool ret = orig_DeleteFileAtPath(&s);
	String_destroy(&s);
	return ret;
}