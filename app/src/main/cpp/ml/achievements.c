#include "ml.h"
#include "java.h"
#include "log.h"
#include "toml.h"
#include "AchievementsManager.h"
#include "std_vector.h"
#include "std_tree.h"
#include "stdstring.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG_TAG "LauncherMLAchievements"

// Offsets
#define PS sizeof(void *)
#define SS (3 * PS)
#define KEY_OFF archSplit(0x10, 0x20)
#define NODE_SIZE (KEY_OFF + SS + 2 * PS)
#define CB_OWNERS PS
#define CB_WEAK (PS + 4)
#define CB_PTR (PS + 8)

// Achievement layout
#define A_ID 0
#define A_TITLE SS
#define A_DESC (2 * SS)
#define A_POINTS (3 * SS)
#define A_STAT archSplit(0x28, 0x50)
#define A_GOAL archSplit(0x34, 0x68)

// AchievementsManager layout
#define M_LIST archSplit(0x00, 0x00)
#define M_BYID archSplit(0x0c, 0x18)
#define M_KEYS archSplit(0x18, 0x30)
#define M_BYKEY archSplit(0x24, 0x48)

static void noop(void) {}
static void *fakeVtbl[6] = {noop, noop, noop, noop, noop, noop};

#define nodeKey(n) ((String *)((char *)(n) + KEY_OFF))

static void treeInsert(std_tree *t, const char *key, void *ptr, void *ctrl, int multi) {
	String tmp;
	String_create(&tmp, key);
	std_tree_node *parent = (std_tree_node *)&t->root;
	std_tree_node **slot = &t->root;
	for (std_tree_node *n = t->root; n;) {
		int c = std_tree_str_cmp(&tmp, nodeKey(n));
		if (c == 0 && !multi) break;
		parent = n;
		slot = c < 0 ? &n->left : &n->right;
		n = *slot;
	}
	String_destroy(&tmp);
	if (*slot) return;
	std_tree_node *n = (std_tree_node *)calloc(1, NODE_SIZE);
	String_create(nodeKey(n), key);
	*(void **)((char *)n + KEY_OFF + SS) = ptr;
	*(void **)((char *)n + KEY_OFF + SS + PS) = ctrl;
	n->parent = parent;
	*slot = n;
	if (t->begin->left) t->begin = t->begin->left;
	std_tree_balance(t->root, n);
	t->size++;
}

void AM_AddAchievement(AchievementsManager *m, const char *id, const char *title, const char *description, int points, const char *counter_name, int counter_value) {
	if (!m) return;
	if (!counter_name) counter_name = "";
	if (!counter_value) counter_value = 1;

	char *mb = (char *)m;
	Achievement *a = (Achievement *)calloc(1, sizeof(Achievement));
	char *ab = (char *)a;

	String_create((String *)(ab + A_ID), id);
	String_create((String *)(ab + A_TITLE), title);
	String_create((String *)(ab + A_DESC), description);
	String_create((String *)(ab + A_STAT), counter_name);
	*(int *)(ab + A_POINTS) = points;
	*(int *)(ab + A_GOAL) = counter_value;

	char *cb = (char *)calloc(1, 4 * PS);
	*(void **)cb = fakeVtbl;
	*(int *)(cb + CB_OWNERS) = 0x40000000;
	*(int *)(cb + CB_WEAK) = 0x40000000;
	*(void **)(cb + CB_PTR) = a;

	void *sp[2] = {a, cb};
	std_vector_push_back((std_vector *)(mb + M_LIST), sp, sizeof(sp));
	treeInsert((std_tree *)(mb + M_BYID), id, a, cb, 0);

	const char *key = *counter_name ? counter_name : id;
	std_tree *byKey = (std_tree *)(mb + M_BYKEY);
	int seen = 0;
	String k;
	String_create(&k, key);
	for (std_tree_node *n = byKey->root; n && !seen;) {
		int c = std_tree_str_cmp(&k, nodeKey(n));
		if (c == 0) seen = 1; else n = c < 0 ? n->left : n->right;
	}
	if (!seen) std_vector_push_back((std_vector *)(mb + M_KEYS), &k, SS);
	else String_destroy(&k);
	treeInsert(byKey, key, a, cb, 1);
}

static void freeNodes(std_tree_node *n) {
	if (!n) return;
	freeNodes(n->left);
	freeNodes(n->right);
	String_destroy(nodeKey(n));
	free(n);
}

void AM_ClearAchievements(AchievementsManager *m) {
	if (!m) return;
	char *mb = (char *)m;

	std_vector *list = (std_vector *)(mb + M_LIST);
	for (char *p = list->begin; p < list->end; p += 2 * PS) {
		Achievement *a = *(Achievement **)p;
		void *cb = *(void **)(p + PS);
		char *ab = (char *)a;
		String_destroy((String *)(ab + A_ID));
		String_destroy((String *)(ab + A_TITLE));
		String_destroy((String *)(ab + A_DESC));
		String_destroy((String *)(ab + A_STAT));
		free(a);
		free(cb);
	}
	std_vector_clear(list);

	std_vector *keys = (std_vector *)(mb + M_KEYS);
	for (char *p = keys->begin; p < keys->end; p += SS) {
		String_destroy((String *)p);
	}
	std_vector_clear(keys);

	std_tree *byId = (std_tree *)(mb + M_BYID);
	freeNodes(byId->root);
	byId->root = NULL;
	byId->begin = (std_tree_node *)&byId->root;
	byId->size = 0;

	std_tree *byKey = (std_tree *)(mb + M_BYKEY);
	freeNodes(byKey->root);
	byKey->root = NULL;
	byKey->begin = (std_tree_node *)&byKey->root;
	byKey->size = 0;
}

static void ML_LoadDefaultAchievements(AchievementsManager *m) {
	AM_AddAchievement(m, "scavenger", "Scavenger", "Find a Treasure.", 10, "treasures", 1);
	AM_AddAchievement(m, "beammeupscotty", "Beam Me Up Scotty", "Travel through a portal.", 10, NULL, 1);
	AM_AddAchievement(m, "angrybats", "Angry Bats", "Kill Szan The Angry.", 20, NULL, 1);
	AM_AddAchievement(m, "blastabat", "Blast a Bat", "Kill a bat with a magic bolt.", 20, NULL, 1);
	AM_AddAchievement(m, "golddigger", "Gold Digger", "Find 10 Treasures.", 20, "treasures", 10);
	AM_AddAchievement(m, "thedestroyer", "The Destroyer", "Smash 100 pots.", 20, "pots", 100);
	AM_AddAchievement(m, "thegardener", "The Gardener", "Cut down 100 bushes.", 20, "bushes", 100);
	AM_AddAchievement(m, "recklessshooting", "Reckless Shooting", "Get killed by your own magic bolt.", 30, NULL, 1);
	AM_AddAchievement(m, "bombabat", "Bomb a Bat", "Kill a bat with a bomb.", 20, NULL, 1);
	AM_AddAchievement(m, "mishandlingexplosives", "Mishandling Explosives", "Get killed by your own bomb.", 30, NULL, 1);
	AM_AddAchievement(m, "pancaketime", "Pancake Time", "Get crushed to death.", 30, NULL, 1);
	AM_AddAchievement(m, "animatedearth", "Animated Earth", "Kill Boulder The Golem.", 30, NULL, 1);
	AM_AddAchievement(m, "lostsword", "Lost Sword", "Find The Needle.", 30, NULL, 1);
	AM_AddAchievement(m, "theundead", "The Undead", "Find The Shadowtrinket.", 30, NULL, 1);
	AM_AddAchievement(m, "lawandjustice", "Law and Justice", "Kill Zak The Bandit Leader.", 30, NULL, 1);
	AM_AddAchievement(m, "likeaninja", "Like a Ninja", "Kill the boss in Florennum without taking any damage.", 80, NULL, 1);
	AM_AddAchievement(m, "duel", "Duel", "Kill Jack The Ambusher.", 30, NULL, 1);
	AM_AddAchievement(m, "anotherentrance", "Another Entrance", "Find The Thorn.", 30, NULL, 1);
	AM_AddAchievement(m, "lordoftheundead", "Lord of The Undead", "Find The Magic Sword.", 30, NULL, 1);
	AM_AddAchievement(m, "toofast", "Too Fast", "Kill Slick The Quick.", 30, NULL, 1);
	AM_AddAchievement(m, "treasurehunter", "Treasure Hunter", "Find 40 Treasures.", 30, "treasures", 40);
	AM_AddAchievement(m, "deadlymage", "Deadly Mage", "Kill Edogani The Deadly.", 30, NULL, 1);
	AM_AddAchievement(m, "toorich", "Too Rich", "Have 999 soul shards.", 30, NULL, 1);
	AM_AddAchievement(m, "takingtheshortcut", "Open Sesame", "Find a Key.", 30, NULL, 1);
	AM_AddAchievement(m, "indianajones", "Indiana Jones", "Find all the Treasures.", 80, "treasures", 59);
	AM_AddAchievement(m, "shatteredblade", "Shattered Blade", "Assemble the Mageblade.", 40, NULL, 1);
	AM_AddAchievement(m, "theexplorer", "The Explorer", "Visit every place.", 30, NULL, 1);
	AM_AddAchievement(m, "masteroforder", "Master of Order", "Kill The Master of Chaos.", 80, NULL, 1);
}

int ML_LoadAchievements(AchievementsManager *m) {
	if (!m) {
		m = AchievementsManager_sharedManager();
		if (!m) {
			LOGE("Failed to get AchievementsManager instance");
			return 0;
		}
	}

	AM_ClearAchievements(m);

	const char *mod_id = java_current_mod_id();
	if (!mod_id || !*mod_id) {
		ML_LoadDefaultAchievements(m);
		LOGI("Loaded vanilla achievements (no mod loaded)");
		return 1;
	}

	const char *ext = java_external_files();
	if (!ext || !ext[0]) {
		ML_LoadDefaultAchievements(m);
		return 0;
	}

	char path[512];
	snprintf(path, sizeof(path), "%s/mods/%s/achievements.toml", ext, mod_id);

	FILE *f = fopen(path, "rb");
	if (!f) {
		ML_LoadDefaultAchievements(m);
		LOGI("Loaded vanilla achievements (no custom file present in mod '%s')", mod_id);
		return 1;
	}

	char errbuf[200];
	toml_table_t *conf = toml_parse_file(f, errbuf, sizeof(errbuf));
	fclose(f);

	if (!conf) {
		LOGE("Failed to parse achievements.toml: %s", errbuf);
		ML_LoadDefaultAchievements(m);
		return 0;
	}

	for (int i = 0; ; i++) {
		const char *key = toml_key_in(conf, i);
		if (!key) break;

		toml_table_t *ach = toml_table_in(conf, key);
		if (!ach) continue;

		const char *id = key;

		toml_datum_t name = toml_string_in(ach, "name");
		toml_datum_t desc = toml_string_in(ach, "description");
		toml_datum_t pts = toml_int_in(ach, "points");
		toml_datum_t cnt = toml_string_in(ach, "counter");
		toml_datum_t thr = toml_int_in(ach, "threshold");

		const char *title_str = name.ok ? name.u.s : "Unknown";
		const char *desc_str = desc.ok ? desc.u.s : "";
		int points = pts.ok ? (int)pts.u.i : 0;
		const char *counter_str = cnt.ok ? cnt.u.s : NULL;
		int threshold = thr.ok ? (int)thr.u.i : 0;

		AM_AddAchievement(m, id, title_str, desc_str, points, counter_str, threshold);

		if (name.ok) free(name.u.s);
		if (desc.ok) free(desc.u.s);
		if (cnt.ok) free(cnt.u.s);
	}

	toml_free(conf);
	LOGI("Loaded custom achievements from achievements.toml for mod '%s'", mod_id);
	return 1;
}

void ML_ReloadAchievements(void) {
	AchievementsManager *m = AchievementsManager_sharedManager();
	if (m) {
		ML_LoadAchievements(m);
	}
}
