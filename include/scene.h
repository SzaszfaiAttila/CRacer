#ifndef SCENE_H
#define SCENE_H

#include "mesh.h"

typedef struct {
    Mesh sea;        // high-res flat grid — animated by water vertex shader
    Mesh arena;
    Mesh arena_acc;
    Mesh pillars;
    Mesh pillar_acc;
    Mesh bike_mesh;
    Mesh moon;       // large emissive disc near the horizon
} Scene;

Scene scene_build(void);
void  scene_free (Scene *s);

#endif
