#include "lua_trace.h"
#include "log.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

// For last 5 function log...

#define LOG_TAG "LuaTrace"
#define TRACE_N 5
#define TRACE_LEN 96

// Pthread, just to be safe
static char g_trace[TRACE_N][TRACE_LEN];
static int g_trace_i;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

void lua_trace_record(const char *name) {
	if (!name || !name[0]) name = "?";
	pthread_mutex_lock(&g_lock);
	snprintf(g_trace[g_trace_i % TRACE_N], TRACE_LEN, "%s", name);
	g_trace_i++;
	pthread_mutex_unlock(&g_lock);
}

void lua_trace_clear(void) {
	pthread_mutex_lock(&g_lock);
	memset(g_trace, 0, sizeof(g_trace));
	g_trace_i = 0;
	pthread_mutex_unlock(&g_lock);
}

void lua_trace_dump_fd(int fd) {
	char line[128];
	pthread_mutex_lock(&g_lock);
	int total = g_trace_i < TRACE_N ? g_trace_i : TRACE_N;
	if (total <= 0) {
		const char *empty = "last lua calls: (none)\n";
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "last lua calls: (none)");
		if (fd >= 0) write(fd, empty, strlen(empty));
		pthread_mutex_unlock(&g_lock);
		return;
	}
	__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "last %d lua call(s):", total);
	if (fd >= 0) {
		int n = snprintf(line, sizeof(line), "last %d lua call(s):\n", total);
		if (n > 0) write(fd, line, (size_t)n);
	}
	for (int k = total - 1; k >= 0; k--) {
		int idx = (g_trace_i - 1 - k + TRACE_N * 8) % TRACE_N;
		const char *name = g_trace[idx][0] ? g_trace[idx] : "?";
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "  [%d] %s", total - k, name);
		if (fd >= 0) {
			int n = snprintf(line, sizeof(line), "  [%d] %s\n", total - k, name);
			if (n > 0) write(fd, line, (size_t)n);
		}
	}
	pthread_mutex_unlock(&g_lock);
}
