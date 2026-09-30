#ifndef LAUNCHER_ACHIEVEMENTSMANAGER_H
#define LAUNCHER_ACHIEVEMENTSMANAGER_H

#include "hook.h"

typedef struct AchievementsManager {
	char _pad0[archSplit(0x24, 0x48)];
	void *tree;
} AchievementsManager;

typedef struct Achievement {
	char _pad[archSplit(0x38, 0x70)];
} Achievement; // sizeof == archSplit(0x38, 0x70)

void AM_AddAchievement(
    AchievementsManager *m,
    const char *id,
    const char *title,
    const char *description,
    int points,
    const char *counter_name,
    int counter_value
);

void AM_ClearAchievements(AchievementsManager *m);

#endif //LAUNCHER_ACHIEVEMENTSMANAGER_H
