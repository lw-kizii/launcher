#include "AchievementsManager.h"
#include "hook.h"
#include "ml.h"

G_DL_SYMBOL(
	AchievementsManager_sharedManager,
	"_ZN5Caver19AchievementsManager13sharedManagerEv",
	AchievementsManager*, (void)
)

HOOK_SYMBOL(
	AchievementsManager_Constructor,
	"_ZN5Caver19AchievementsManagerC2Ev",
	void, (AchievementsManager *this)
) {
	orig_AchievementsManager_Constructor(this);
	ML_LoadAchievements(this);
}
