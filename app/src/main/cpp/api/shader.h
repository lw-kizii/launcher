/*
 * Contributed by Raijin
 */
#ifndef LAUNCHER_SHADER_H
#define LAUNCHER_SHADER_H

#include "lua.h"

void API_register_shader(lua_State *L);
void shader_on_scene_drawn(void *scene);
void shader_cleanup(void);

#endif
