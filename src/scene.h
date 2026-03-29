#ifndef SCENE_H
#define SCENE_H

#include "mesh.h"

typedef struct {
    Mesh sea;
    Mesh arena;
    Mesh arena_acc;
    Mesh pillars;
    Mesh pillar_acc;
    Mesh bike_mesh;
} Scene;

Scene scene_build(void);
void  scene_free (Scene *s);

#endif