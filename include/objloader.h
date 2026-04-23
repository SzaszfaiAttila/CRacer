#ifndef OBJLOADER_H
#define OBJLOADER_H

#include "mesh.h"

/*  ObjModel — a GPU mesh with the MTL diffuse colour baked in.
 *
 *  kd_r / kd_g / kd_b  = Kd from the first material in the .mtl file,
 *                         or (1,1,1) if no .mtl / no Kd is found.
 *  ks_r / ks_g / ks_b  = Ks (specular colour), or (1,1,1).
 *  ns                   = Ns (shininess exponent), or 32.
 *
 *  mesh.vert_count == 0 means loading failed; callers must check.            */
typedef struct {
    Mesh  mesh;
    float kd_r, kd_g, kd_b;
    float ks_r, ks_g, ks_b;
    float ns;
} ObjModel;

/*  Load a Wavefront OBJ (+MTL) from disk.
 *  Supports: v  vn  f  mtllib  usemtl
 *  Face topology: any n-gon, fan-triangulated.
 *  OBJ index convention: 1-based and negative indices both handled.          */
ObjModel obj_load(const char *path);

/*  Free the GPU resources held by the model.                                 */
void obj_free(ObjModel *m);

#endif /* OBJLOADER_H */
