#ifndef PARTICLES_H
#define PARTICLES_H

#include <cglm/cglm.h>
#include "config.h"
#include "bike.h"          // for Direction

extern float part_buf[PART_BUF_FLOATS];

void particles_init   (void);
void particles_update (float dt);
int  particles_build  (vec3 cam_right, vec3 cam_up);

// ── Death explosion particles ────────────────────────────────────────────────
void particles_spawn_death_cloud(float px, float pz, Direction dir,
                                 float speed,
                                 float base_r, float base_g, float base_b,
                                 float spawn_y);

void particles_update_death(float dt);

#endif
