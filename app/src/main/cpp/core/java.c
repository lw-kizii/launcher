#include "java.h"

#include <stdio.h>
#include <string.h>
#include "log.h"

#define LOG_TAG "LauncherJava"

static JavaVM *g_vm = NULL;
static jclass g_mainActivityClass = NULL;
static jmethodID g_currentModMethod = NULL;

static Paths g_paths = {{0}};
static ModInfo g_mod_info = {{0}};
static int g_mod_fetched = 0;

JavaVM *java_get_vm(void) {
	return g_vm;
}

JNIEnv *java_get_env(int *out_attached) {
	if (out_attached) *out_attached = 0;
	if (!g_vm) return NULL;

	JNIEnv *env = NULL;
	int status = (*g_vm)->GetEnv(g_vm, (void **)&env, JNI_VERSION_1_6);

	if (status == JNI_EDETACHED) {
		if ((*g_vm)->AttachCurrentThread(g_vm, &env, NULL) != 0) {
			LOGE("AttachCurrentThread failed");
			return NULL;
		}
		if (out_attached) *out_attached = 1;
	} else if (status != JNI_OK) {
		LOGE("GetEnv failed (%d)", status);
		return NULL;
	}
	return env;
}

void java_release_env(int attached) {
	if (attached && g_vm)
		(*g_vm)->DetachCurrentThread(g_vm);
}

const Paths *java_get_paths(void) {
	return &g_paths;
}

const ModInfo *java_get_mod_info(void) {
	if (!g_mod_fetched) {
		java_current_mod_id();
	}
	return &g_mod_info;
}

const char *java_internal_files(void) {
	return g_paths.internalfiles;
}

const char *java_internal_cache(void) {
	return g_paths.internalcache;
}

const char *java_external_files(void) {
	return g_paths.externalfiles;
}

static void fetch_current_mod(void) {
	if (!g_vm || !g_mainActivityClass || !g_currentModMethod) return;

	int attached = 0;
	JNIEnv *env = java_get_env(&attached);
	if (!env) return;

	jstring jmod = (jstring)(*env)->CallStaticObjectMethod(
		env, g_mainActivityClass, g_currentModMethod);

	if (jmod) {
		const char *chars = (*env)->GetStringUTFChars(env, jmod, NULL);
		if (chars) {
			snprintf(g_mod_info.id, sizeof(g_mod_info.id), "%s", chars);
			(*env)->ReleaseStringUTFChars(env, jmod, chars);
		}
		(*env)->DeleteLocalRef(env, jmod);
	} else {
		g_mod_info.id[0] = '\0';
	}

	if (g_mod_info.id[0]) {
		snprintf(g_mod_info.rootPath, sizeof(g_mod_info.rootPath), "%s/mods/%s", g_paths.externalfiles, g_mod_info.id);
	} else {
		g_mod_info.rootPath[0] = '\0';
	}

	java_release_env(attached);

	LOGI("current mod: '%s'", g_mod_info.id[0] ? g_mod_info.id : "(vanilla)");
}

const char *java_current_mod_id_safe(void) {
	return g_mod_fetched && g_mod_info.id[0] ? g_mod_info.id : "";
}

const char *java_current_mod_id(void) {
	if (!g_mod_fetched) {
		fetch_current_mod();
		g_mod_fetched = 1;
	}
	return g_mod_info.id;
}

const char *java_mod_resource_path(const char *res) {
	const char *id = java_current_mod_id();
	static char buf[512];
	snprintf(buf, sizeof(buf),
			 "%s/mods/%s/%s",
			 g_paths.externalfiles, id, res);
	return buf;
}

void java_reset_mod_id(void) {
	g_mod_info.id[0] = '\0';
	g_mod_info.rootPath[0] = '\0';
	g_mod_fetched = 0;
}

void java_set_main_activity(jclass clazz, jmethodID current_mod_method) {
	g_mainActivityClass = clazz;
	g_currentModMethod = current_mod_method;
}

static void copyjstring(JNIEnv *env, jstring src, char *dst) {
	if (src) {
		const char *p = (*env)->GetStringUTFChars(env, src, NULL);
		if (p) {
			snprintf(dst, 256, "%s", p);
			(*env)->ReleaseStringUTFChars(env, src, p);
		}
	}
}

JNIEXPORT void JNICALL
Java_net_kiwi_launcher_API_initPaths(JNIEnv *env, jclass clazz,
											  jstring internalFiles, jstring internalCache, jstring externalFiles)
{
	copyjstring(env, internalFiles, g_paths.internalfiles);
	copyjstring(env, externalFiles, g_paths.externalfiles);
	copyjstring(env, internalCache, g_paths.internalcache);

	(*env)->GetJavaVM(env, &g_vm);

	jclass global = (jclass)(*env)->NewGlobalRef(env, clazz);
	jmethodID method = (*env)->GetStaticMethodID(env, global, "currentMod", "()Ljava/lang/String;");

	if (!method) {
		LOGE("API.currentMod() not found");
		(*env)->ExceptionClear(env);
	}

	java_set_main_activity(global, method);

	LOGI("--- Paths ---\n internal=%s\n internalcache=%s\n external=%s\n--- ..... ---", g_paths.internalfiles, g_paths.internalcache, g_paths.externalfiles);
}
