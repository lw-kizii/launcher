#include <jni.h>
#include <string.h>
#include "core/hook.h"
#include "core/core.h"
#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "log.h"

#define LOG_TAG "LauncherMain"

extern void assets_on_mod_exit(void);
extern void saves_on_mod_exit(void);
extern void init_saves(void);

extern void init_API();

//extern void init_jpatch(void);

JNIEXPORT void JNICALL
Java_net_kiwi_launcher_MainActivity_loadHooks(JNIEnv *env, jclass clazz) {
	init_crasher();
	init_hooks();
	init_assets();
	load_mod_libraries();
	init_lua();
	init_lual();
	init_saves();
	init_API();
}

JNIEXPORT void JNICALL
Java_net_kiwi_launcher_MainActivity_onModExit(JNIEnv *env, jclass clazz) {
	assets_on_mod_exit();
	saves_on_mod_exit();
}

JNIEXPORT void JNICALL
Java_net_kiwi_launcher_MainActivity_init(JNIEnv *env, jclass clazz) {
}
