#ifndef LAWNCHER_LUA_TRACE_H
#define LAWNCHER_LUA_TRACE_H

#ifdef __cplusplus
extern "C" {
#endif

void lua_trace_record(const char *name);
void lua_trace_clear(void);
/* writes lines to fd using write(); fd may be -1 to only logcat via callback pattern in crasher */
void lua_trace_dump_fd(int fd);

#ifdef __cplusplus
}
#endif

#endif
