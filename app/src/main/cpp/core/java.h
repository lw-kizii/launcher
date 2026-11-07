#ifndef LAWNCHER_CORE_JAVA_H
#define LAWNCHER_CORE_JAVA_H

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Paths {
	char internalfiles[256];
	char internalcache[256];
	char externalfiles[256];
} Paths;

typedef struct ModInfo {
	char rootPath[256];
	char id[128];
} ModInfo;

JavaVM *java_get_vm(void);
JNIEnv *java_get_env(int *out_attached);
void java_release_env(int attached);

const Paths *java_get_paths(void);
const ModInfo *java_get_mod_info(void);

const char *java_internal_files(void);
const char *java_internal_cache(void);
const char *java_external_files(void);
const char *java_current_mod_id(void);
const char *java_current_mod_id_safe(void);
const char *java_mod_resource_path(const char *res);

void java_reset_mod_id(void);
void java_set_main_activity(jclass clazz, jmethodID current_mod_method);

#ifdef __cplusplus
}
#endif

#endif
