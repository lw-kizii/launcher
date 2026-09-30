#include <jni.h>
#include <string.h>
#include "core/hook.h"
#include "core/core.h"
#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "log.h"

#define LOG_TAG "LauncherMain"

#include "ml/ml.h"

extern void init_API();

JNIEXPORT void JNICALL
Java_net_kiwi_launcher_MainActivity_loadHooks(JNIEnv *env, jclass clazz) {
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
}

JNIEXPORT void JNICALL
Java_net_kiwi_launcher_MainActivity_init(JNIEnv *env, jclass clazz) {
}
