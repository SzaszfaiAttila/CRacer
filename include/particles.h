#ifndef PARTICLES_H
#define PARTICLES_H

#include <cglm/cglm.h>
#include "config.h"
#include "bike.h"

extern float part_buf[PART_BUF_FLOATS];

void particles_init   (void);
void particles_update (float dt);
int  particles_build  (vec3 cam_right, vec3 cam_up);

/* ── Death explosion particles ───────────────────────────────────────────── */
void particles_spawn_death_cloud(float px, float pz, Direction dir,
                                 float speed,
                                 float base_r, float base_g, float base_b,
                                 float spawn_y);

void particles_update_death(float dt);

/* ── Dash spark particles ────────────────────────────────────────────────── */
/*  rate_level:
 *    0 = no sparks (cooldown)
 *    1 = trickle  (ready to dash)
 *    2 = burst    (currently dashing)
 *  cr/cg/cb = base spark colour
 */
void particles_emit_dash(float px, float py, float pz,
                         Direction dir,
                         float cr, float cg, float cb,
                         int rate_level, float dt);

void particles_update_dash(float dt);

#endif
