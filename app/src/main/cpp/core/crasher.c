#include "core.h"
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <ucontext.h>
#include <dlfcn.h>
#include <unwind.h>
#include "log.h"
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <jni.h>
#include <sys/types.h>

#define LOG_TAG "NativeCrashCatcher"

#if defined(__arm__)
	#define GET_PC(ctx) ((ctx)->uc_mcontext.arm_pc)
	#define GET_LR(ctx) ((ctx)->uc_mcontext.arm_lr)
	#define GET_SP(ctx) ((ctx)->uc_mcontext.arm_sp)
#elif defined(__aarch64__)
	#define GET_PC(ctx) ((ctx)->uc_mcontext.pc)
	#define GET_LR(ctx) ((ctx)->uc_mcontext.regs[30])
	#define GET_SP(ctx) ((ctx)->uc_mcontext.sp)
#endif

static struct sigaction g_old_sa[NSIG];

static char g_crash_log_path[512] = {0};
static char g_altstack[SIGSTKSZ];
struct BacktraceState {
	int current_depth;
	int fd;
	int max_depth;
};

static const char *get_basename(const char *path) {
	if (!path) return "unknown_lib";
	const char *slash = strrchr(path, '/');
	return slash ? slash + 1 : path;
}

static void log_both(int fd, const char *fmt, ...) {
	char buf[768];
	va_list ap;
	va_start(ap, fmt);
	int n = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (n <= 0) return;
	if (n >= (int)sizeof(buf)) n = (int)sizeof(buf) - 1;
	__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "%s", buf);
	if (fd >= 0) {
		write(fd, buf, (size_t)n);
		write(fd, "\n", 1);
	}
}

static void log_frame(int fd, int depth, uintptr_t pc) {
	if (!pc) {
		log_both(fd, "\t#%02d pc 00000000  <null>", depth);
		return;
	}
	Dl_info info;
	if (dladdr((void*)pc, &info) != 0 && info.dli_fname) {
		const char *lib = get_basename(info.dli_fname);
		uintptr_t off = pc - (uintptr_t)info.dli_fbase;
		if (info.dli_sname) {
			uintptr_t sym_off = pc - (uintptr_t)info.dli_saddr;
			log_both(fd, "\t#%02d pc %08zx  %s (%s+0x%zx)", depth, off, lib, info.dli_sname, sym_off);
		} else {
			log_both(fd, "\t#%02d pc %08zx  %s", depth, off, lib);
		}
	} else {
		log_both(fd, "\t#%02d pc %08zx  <unknown>", depth, pc);
	}
}

static _Unwind_Reason_Code unwind_callback(struct _Unwind_Context *context, void *arg) {
	struct BacktraceState *state = (struct BacktraceState *)arg;
	if (state->current_depth >= state->max_depth) return _URC_END_OF_STACK;
	uintptr_t pc = _Unwind_GetIP(context);
	if (pc && state->current_depth > 0) log_frame(state->fd, state->current_depth, pc);
	state->current_depth++;
	return _URC_NO_REASON;
}

static void native_crash_handler(int sig, siginfo_t *info, void *context) {
	ucontext_t *uc = (ucontext_t *)context;
	uintptr_t pc = GET_PC(uc);
	uintptr_t lr = GET_LR(uc);
	uintptr_t sp = GET_SP(uc);
	int fd = -1;
	if (g_crash_log_path[0] != '\0') fd = open(g_crash_log_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	time_t now = time(NULL);
	char tbuf[64];
	strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", localtime(&now));
	log_both(fd, "===== NCC");
	log_both(fd, "Native Crash Catcher  pid:%d  tid:%d", getpid(), gettid());
	log_both(fd, "Timestamp: %s", tbuf);
	log_both(fd, "signal %d (%s), code %d, fault addr %p", sig, strsignal(sig), info->si_code, info->si_addr);
	log_both(fd, "\tpc  %016zx  lr  %016zx  sp  %016zx", pc, lr, sp);
	log_both(fd, "backtrace:");
	log_frame(fd, 0, pc);
	if (lr && lr != pc) log_frame(fd, 1, lr);
	struct BacktraceState state = {2, fd, 32};
	_Unwind_Backtrace(unwind_callback, &state);
	log_both(fd, "===== NCC End =====");
	if (fd >= 0) {
		fsync(fd);
		close(fd);
	}
	if (g_old_sa[sig].sa_flags & SA_SIGINFO) {
		if (g_old_sa[sig].sa_sigaction) g_old_sa[sig].sa_sigaction(sig, info, context);
	} else if (g_old_sa[sig].sa_handler != SIG_DFL && g_old_sa[sig].sa_handler != SIG_IGN) {
		g_old_sa[sig].sa_handler(sig);
	} else {
		signal(sig, SIG_DFL);
		raise(sig);
	}
}

void init_crasher(void) {
	stack_t ss = {.ss_sp = g_altstack, .ss_size = sizeof(g_altstack), .ss_flags = 0};
	sigaltstack(&ss, NULL);
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND;
	sa.sa_sigaction = native_crash_handler;
	sigemptyset(&sa.sa_mask);
	int signals[] = {SIGSEGV, SIGABRT, SIGILL, SIGFPE, SIGBUS};
	for (int i = 0; i < (int)(sizeof(signals) / sizeof(signals[0])); i++) sigaction(signals[i], &sa, &g_old_sa[signals[i]]);
	LOGI("Crash handler installed!");
}

// MainActivity.setCrashLogPath
void set_crash_log_path(const char *path) {
	if (!path) {
		g_crash_log_path[0] = '\0';
		return;
	}
	strncpy(g_crash_log_path, path, sizeof(g_crash_log_path) - 1);
	g_crash_log_path[sizeof(g_crash_log_path) - 1] = '\0';
	LOGI("Crash log path set to %s", g_crash_log_path);
}

JNIEXPORT void JNICALL
Java_net_kiwi_launcher_MainActivity_setCrashLogPath(JNIEnv *env, jclass clazz, jstring path) {
	(void)clazz; // Not necessary.
	if (!path) {
		set_crash_log_path(NULL);
		return;
	}
	const char *crash_path = (*env)->GetStringUTFChars(env, path, NULL);
	set_crash_log_path(crash_path);
	(*env)->ReleaseStringUTFChars(env, path, crash_path);
}