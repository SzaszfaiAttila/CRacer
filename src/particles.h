#ifndef PARTICLES_H
#define PARTICLES_H

#include <cglm/cglm.h>
#include "config.h"

extern float part_buf[PART_BUF_FLOATS];

void particles_init   (void);
void particles_update (float dt);
int  particles_build  (vec3 cam_right, vec3 cam_up); // fills part_buf, returns float count

#endif