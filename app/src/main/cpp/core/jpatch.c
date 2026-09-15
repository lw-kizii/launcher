#include "java.h"
#include "hook.h"
#include "stdstring.h"
#include "log.h"

#include <jni.h>
#include <stdbool.h>
#include <string.h>

HOOK_SYMBOL(
	AndroidIsGoogleGameServicesAvailable,
	"_ZN5Caver36AndroidIsGoogleGameServicesAvailableEv",
	bool, (void)
) {
	return false;
}

#define LOG_TAG "JavaPatch"

// I *need* these.
static jclass g_port;
static jmethodID g_loadFile;
static jmethodID g_play;
static jmethodID g_pause;
static jmethodID g_stop;
static jmethodID g_setLooping;
static jmethodID g_setVolume;

static int port_ready(JNIEnv *env) {
	if (g_port) return 1;
	jclass local = (*env)->FindClass(env, "net/kiwi/launcher/Port");
	if (!local) {
		(*env)->ExceptionClear(env);
		return 0;
	}
	g_port = (*env)->NewGlobalRef(env, local);
	(*env)->DeleteLocalRef(env, local);
	g_loadFile = (*env)->GetStaticMethodID(env, g_port, "loadFile", "(Ljava/lang/String;)Z");
	g_play = (*env)->GetStaticMethodID(env, g_port, "play", "()V");
	g_pause = (*env)->GetStaticMethodID(env, g_port, "pause", "()V");
	g_stop = (*env)->GetStaticMethodID(env, g_port, "stop", "()V");
	g_setLooping = (*env)->GetStaticMethodID(env, g_port, "setLooping", "(Z)V");
	g_setVolume = (*env)->GetStaticMethodID(env, g_port, "setVolume", "(F)V");
	if (!g_loadFile || !g_play || !g_pause || !g_stop || !g_setLooping || !g_setVolume) {
		(*env)->ExceptionClear(env);
		return 0;
	}
	return 1;
}

static bool call_load(const char *track) {
	int attached = 0;
	JNIEnv *env = java_get_env(&attached);
	if (!env || !port_ready(env)) {
		java_release_env(attached);
		return false;
	}
	jstring jtrack = (*env)->NewStringUTF(env, track ? track : "");
	jboolean ok = (*env)->CallStaticBooleanMethod(env, g_port, g_loadFile, jtrack);
	if (jtrack) (*env)->DeleteLocalRef(env, jtrack);
	if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
	java_release_env(attached);
	return ok == JNI_TRUE;
}

static void call_void(jmethodID mid) {
	int attached = 0;
	JNIEnv *env = java_get_env(&attached);
	if (!env || !port_ready(env)) {
		java_release_env(attached);
		return;
	}
	(*env)->CallStaticVoidMethod(env, g_port, mid);
	if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
	java_release_env(attached);
}

static void call_set_looping(bool loop) {
	int attached = 0;
	JNIEnv *env = java_get_env(&attached);
	if (!env || !port_ready(env)) {
		java_release_env(attached);
		return;
	}
	(*env)->CallStaticVoidMethod(env, g_port, g_setLooping, loop ? JNI_TRUE : JNI_FALSE);
	if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
	java_release_env(attached);
}

static void call_set_volume(float vol) {
	int attached = 0;
	JNIEnv *env = java_get_env(&attached);
	if (!env || !port_ready(env)) {
		java_release_env(attached);
		return;
	}
	(*env)->CallStaticVoidMethod(env, g_port, g_setVolume, vol);
	if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
	java_release_env(attached);
}

HOOK_SYMBOL(
	MusicLoadFile,
	"_ZN14MusicPlayerJNI8LoadFileERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE",
	bool, (String *s)
) {
	const char *track = s ? String_get(s) : "";
	return call_load(track);
}

HOOK_SYMBOL(
	MusicPlay,
	"_ZN14MusicPlayerJNI4PlayEv",
	void, (void)
) {
	call_void(g_play);
}

HOOK_SYMBOL(
	MusicPause,
	"_ZN14MusicPlayerJNI5PauseEv",
	void, (void)
) {
	call_void(g_pause);
}

HOOK_SYMBOL(
	MusicStop,
	"_ZN14MusicPlayerJNI4StopEv",
	void, (void)
) {
	call_void(g_stop);
}

HOOK_SYMBOL(
	MusicSetLooping,
	"_ZN14MusicPlayerJNI10SetLoopingEb",
	void, (bool loop)
) {
	call_set_looping(loop);
}

HOOK_SYMBOL(
	MusicSetVolume,
	"_ZN14MusicPlayerJNI9SetVolumeEf",
	void, (float vol)
) {
	call_set_volume(vol);
}
