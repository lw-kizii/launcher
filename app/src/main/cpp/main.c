#include <jni.h>
#include <string.h>
#include <android/log.h>
#include <stdarg.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include "core/hook.h"
#include "core/core.h"
#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "log.h"

#define LOG_TAG "LauncherMain"

#include "ml/ml.h"
#include "lua_trace.h"

extern void init_API();

static int (*orig_log_print)(int prio, const char *tag, const char *fmt, ...) = NULL;
static int (*orig_log_vprint)(int prio, const char *tag, const char *fmt, va_list ap) = NULL;
static int (*orig_log_write)(int prio, const char *tag, const char *text) = NULL;

static int hook_log_print(int prio, const char *tag, const char *fmt, ...) {
	if (tag && (!strcmp(tag, "ANGLE") || !strcmp(tag, "libEGL"))) return 0;
	if (!orig_log_print) return 0;
	va_list ap;
	va_start(ap, fmt);
	char buf[1024];
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	return orig_log_print(prio, tag, "%s", buf);
}

static int hook_log_vprint(int prio, const char *tag, const char *fmt, va_list ap) {
	if (tag && (!strcmp(tag, "ANGLE") || !strcmp(tag, "libEGL"))) return 0;
	if (!orig_log_vprint) return 0;
	return orig_log_vprint(prio, tag, fmt, ap);
}

static int hook_log_write(int prio, const char *tag, const char *text) {
	if (tag && (!strcmp(tag, "ANGLE") || !strcmp(tag, "libEGL"))) return 0;
	if (!orig_log_write) return 0;
	return orig_log_write(prio, tag, text);
}

JNIEXPORT void JNICALL
Java_net_kiwi_launcher_MainActivity_loadHooks(JNIEnv *env, jclass clazz) {
	GlossInit(true);
	GlossHookByName("liblog.so", "__android_log_print", (void *)hook_log_print, (void **)&orig_log_print, NULL);
	GlossHookByName("liblog.so", "__android_log_vprint", (void *)hook_log_vprint, (void **)&orig_log_vprint, NULL);
	GlossHookByName("liblog.so", "__android_log_write", (void *)hook_log_write, (void **)&orig_log_write, NULL);
	init_crasher();
	init_hooks();
	init_lua();
	init_lual();
	init_API();
	ML_init();
}

JNIEXPORT void JNICALL
Java_net_kiwi_launcher_MainActivity_onModExit(JNIEnv *env, jclass clazz) {
	ML_exit();
	lua_trace_clear();
}

JNIEXPORT void JNICALL
Java_net_kiwi_launcher_MainActivity_init(JNIEnv *env, jclass clazz) {
}
