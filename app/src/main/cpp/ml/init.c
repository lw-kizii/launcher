#include "ml.h"
#include "java.h"
#include "log.h"

#define LOG_TAG "LauncherML"

void ML_init(void) {
	LOGI("Start of ML Override");
	ML_load_mod_libraries();
	LOGI("ML Override ready");
}

void ML_exit(void) {
	ML_unload_mod_libraries();
	java_reset_mod_id();
	LOGI("ML_exit: state cleared");
}
