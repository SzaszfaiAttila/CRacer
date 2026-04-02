#ifndef MESH_H
#define MESH_H

#include <glad/gl.h>

// ── GPU mesh types ─────────────────────────────────────────────────────────────
typedef struct { GLuint vao, vbo; int vert_count; }              Mesh;
typedef struct { GLuint vao, vbo; int vert_count; int max_floats; } DynMesh;
typedef struct { GLuint vao, vbo; int vert_count; }              PMesh; // pos+rgba, no normal

// Mesh lifecycle
Mesh     mesh_create    (float *v, int float_count);
void     mesh_draw      (Mesh *m);
void     mesh_free      (Mesh *m);

DynMesh  dynmesh_create (int max_floats);
void     dynmesh_update (DynMesh *m, float *data, int float_count);
void     dynmesh_draw   (DynMesh *m);
void     dynmesh_free   (DynMesh *m);

PMesh    pmesh_create   (int max_floats);
void     pmesh_update   (PMesh *m, float *data, int float_count);
void     pmesh_draw     (PMesh *m);
void     pmesh_free     (PMesh *m);

// ── Primitive geometry builders ───────────────────────────────────────────────
// All append into caller-provided float buffer and return updated offset.
// Vertex format: x y z  nx ny nz  (6 floats per vertex)
int add_quad_y  (float *b, int off,
                 float x0, float z0, float x1, float z1, float y);

int add_box     (float *b, int off,
                 float x0, float y0, float z0,
                 float x1, float y1, float z1);

int add_frustum (float *b, int off,
                 float cx, float cz,
                 float bh, float th,
                 float y0, float y1);

#endif