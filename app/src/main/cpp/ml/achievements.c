#include "ml.h"
#include "java.h"
#include "log.h"
#include "toml.h"
#include "AchievementsManager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG_TAG "LauncherMLAchievements"

int ML_LoadAchievements(AchievementsManager *m) {
    const char *mod_id = java_current_mod_id();
    if (!mod_id || !*mod_id) return 0;

    const char *ext = java_external_files();
    if (!ext || !ext[0]) return 0;

    char path[512];
    snprintf(path, sizeof(path), "%s/mods/%s/achievements.toml", ext, mod_id);

    FILE *f = fopen(path, "rb");
    if (!f) return 0; // File doesn't exist

    char errbuf[200];
    toml_table_t *conf = toml_parse_file(f, errbuf, sizeof(errbuf));
    fclose(f);

    if (!conf) {
        LOGE("Failed to parse achievements.toml: %s", errbuf);
        return 0; // Parse error
    }

    // Success parsing. Let's clear vanilla achievements!
    AM_ClearAchievements(m);

    // Loop through root table keys (each key is an achievement ID)
    for (int i = 0; ; i++) {
        const char *key = toml_key_in(conf, i);
        if (!key) break;

        toml_table_t *ach = toml_table_in(conf, key);
        if (!ach) continue; // Should be a table

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
    LOGI("Loaded custom achievements from achievements.toml");
    return 1;
}