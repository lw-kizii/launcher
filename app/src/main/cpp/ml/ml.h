#ifndef LAUNCHER_ML_H
#define LAUNCHER_ML_H

#include "stdstring.h"
#include <stdio.h>

const char *ML_path_basename(const char *path);
void ML_ensure_dir(const char *path);

FILE *ML_fetch_asset(String *asset);

int ML_is_save_ext(const char *ext);
int ML_is_save_path(const char *p);
void ML_redirect_path(String *out, const char *orig);

void ML_init(void);
void ML_exit(void);

void ML_load_mod_libraries(void);
void ML_unload_mod_libraries(void);

struct AchievementsManager;
int ML_LoadAchievements(struct AchievementsManager *m);
void ML_ReloadAchievements(void);

#endif //LAUNCHER_ML_H
