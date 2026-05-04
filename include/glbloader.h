#ifndef GLBLOADER_H
#define GLBLOADER_H

#include "mesh.h"
#include <glad/gl.h>

/* ── Per-primitive sub-mesh ─────────────────────────────────────────────── */
/* Each GLTF primitive maps to one GPU mesh + one set of PBR material values */
#define GLB_MAX_PRIMS 32

typedef struct {
    Mesh  mesh;
    float kd[3];          /* base colour (Principled BSDF Base Color)        */
    float emissive[3];    /* emissive factor — bright parts glow in the dark */
    float roughness;      /* 0 = mirror, 1 = matte                           */
    float metallic;       /* 0 = plastic, 1 = metal                          */
    float alpha;          /* base colour alpha (1 = opaque)                  */
} GlbPrim;

/* ── Model ──────────────────────────────────────────────────────────────── */
typedef struct {
    GlbPrim prims[GLB_MAX_PRIMS];
    int     prim_count;

    /* Aggregate values (weighted average across all primitives) —
       used by the reflection pass which needs a single colour/shininess.    */
    float kd_r, kd_g, kd_b;
    float ks_r, ks_g, ks_b;
    float ns;
} ObjModel;

/* ── API ─────────────────────────────────────────────────────────────────── */

/* Load a .glb file.  Returns a model with prim_count == 0 on failure.
   Requires cgltf.h in your include path (MIT, single header):
     https://github.com/jkuhlmann/cgltf                                      */
ObjModel obj_load(const char *path);

/* Draw all primitives with correct per-material shader uniforms.
   Expects the main scene shader (sh) to already be bound via shader_use().
   Sets: objectColor, emissive, ambientStr, uSpecularStr, uShininess.
   brightness multiplies emissive so g_brightness is applied consistently.
   Disables GL_CULL_FACE during draw and restores it after — this fixes
   see-through on models whose normals were exported pointing inward.         */
void glb_draw_model(const ObjModel *m, GLuint sh, float brightness);

/* Free all GPU resources held by the model. */
void obj_free(ObjModel *m);

#endif /* GLBLOADER_H */
