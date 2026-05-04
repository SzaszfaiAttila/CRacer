#include "mesh.h"
#include <stdlib.h>
#include <math.h>

// ══════════════════════════════════════════════════════════════ MESH LIFECYCLE

Mesh mesh_create(float *v, int n) {
    Mesh m; m.vert_count = n / 6;
    glGenVertexArrays(1, &m.vao); glGenBuffers(1, &m.vbo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, n * sizeof(float), v, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    return m;
}

void mesh_draw(Mesh *m) {
    glBindVertexArray(m->vao);
    glDrawArrays(GL_TRIANGLES, 0, m->vert_count);
    glBindVertexArray(0);
}

void mesh_free(Mesh *m) {
    glDeleteVertexArrays(1, &m->vao);
    glDeleteBuffers(1, &m->vbo);
}

DynMesh dynmesh_create(int max_floats) {
    DynMesh m; m.vert_count = 0; m.max_floats = max_floats;
    glGenVertexArrays(1, &m.vao); glGenBuffers(1, &m.vbo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, max_floats * sizeof(float), NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    return m;
}

void dynmesh_update(DynMesh *m, float *data, int float_count) {
    m->vert_count = float_count / 6;
    glBindBuffer(GL_ARRAY_BUFFER, m->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, float_count * sizeof(float), data);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void dynmesh_draw(DynMesh *m) {
    if (!m->vert_count) return;
    glBindVertexArray(m->vao);
    glDrawArrays(GL_TRIANGLES, 0, m->vert_count);
    glBindVertexArray(0);
}

void dynmesh_free(DynMesh *m) {
    glDeleteVertexArrays(1, &m->vao);
    glDeleteBuffers(1, &m->vbo);
}

PMesh pmesh_create(int max_floats) {
    PMesh m; m.vert_count = 0;
    glGenVertexArrays(1, &m.vao); glGenBuffers(1, &m.vbo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, max_floats * sizeof(float), NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    return m;
}

void pmesh_update(PMesh *m, float *data, int float_count) {
    m->vert_count = float_count / 7;
    glBindBuffer(GL_ARRAY_BUFFER, m->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, float_count * sizeof(float), data);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void pmesh_draw(PMesh *m) {
    if (!m->vert_count) return;
    glBindVertexArray(m->vao);
    glDrawArrays(GL_TRIANGLES, 0, m->vert_count);
    glBindVertexArray(0);
}

void pmesh_free(PMesh *m) {
    glDeleteVertexArrays(1, &m->vao);
    glDeleteBuffers(1, &m->vbo);
}

// ══════════════════════════════════════════════════════ GEOMETRY BUILDERS

int add_quad_y(float *b, int off,
               float x0, float z0, float x1, float z1, float y) {
    float q[6][6] = {
        {x0,y,z0,0,1,0},{x1,y,z0,0,1,0},{x0,y,z1,0,1,0},
        {x1,y,z0,0,1,0},{x1,y,z1,0,1,0},{x0,y,z1,0,1,0},
    };
    for (int i=0;i<6;i++) for (int j=0;j<6;j++) b[off++]=q[i][j];
    return off;
}

int add_box(float *b, int off,
            float x0, float y0, float z0,
            float x1, float y1, float z1) {
    typedef struct { float n[3]; float v[4][3]; } Face;
    Face faces[6] = {
        {{ 0, 1, 0},{{x0,y1,z0},{x1,y1,z0},{x1,y1,z1},{x0,y1,z1}}},
        {{ 0,-1, 0},{{x0,y0,z1},{x1,y0,z1},{x1,y0,z0},{x0,y0,z0}}},
        {{ 1, 0, 0},{{x1,y0,z0},{x1,y0,z1},{x1,y1,z1},{x1,y1,z0}}},
        {{-1, 0, 0},{{x0,y0,z1},{x0,y0,z0},{x0,y1,z0},{x0,y1,z1}}},
        {{ 0, 0,-1},{{x1,y0,z0},{x0,y0,z0},{x0,y1,z0},{x1,y1,z0}}},
        {{ 0, 0, 1},{{x0,y0,z1},{x1,y0,z1},{x1,y1,z1},{x0,y1,z1}}},
    };
    int tris[2][3]={{0,1,2},{0,2,3}};
    for (int f=0;f<6;f++) for (int t=0;t<2;t++) for (int k=0;k<3;k++) {
        float *vv=faces[f].v[tris[t][k]];
        b[off++]=vv[0]; b[off++]=vv[1]; b[off++]=vv[2];
        b[off++]=faces[f].n[0]; b[off++]=faces[f].n[1]; b[off++]=faces[f].n[2];
    }
    return off;
}

int add_frustum(float *b, int off,
                float cx, float cz, float bh, float th,
                float y0, float y1) {
    typedef struct { float v[4][3]; } SFace;
    /* Vertex order reversed vs original so cross-product normals point outward */
    SFace sides[4] = {
        {{{cx+th,y1,cz-th},{cx+th,y1,cz+th},{cx+bh,y0,cz+bh},{cx+bh,y0,cz-bh}}},
        {{{cx-th,y1,cz+th},{cx-th,y1,cz-th},{cx-bh,y0,cz-bh},{cx-bh,y0,cz+bh}}},
        {{{cx+th,y1,cz+th},{cx-th,y1,cz+th},{cx-bh,y0,cz+bh},{cx+bh,y0,cz+bh}}},
        {{{cx-th,y1,cz-th},{cx+th,y1,cz-th},{cx+bh,y0,cz-bh},{cx-bh,y0,cz-bh}}},
    };
    int tris[2][3]={{0,1,2},{0,2,3}};
    for (int f=0;f<4;f++) {
        float *v0=sides[f].v[0], *v1=sides[f].v[1], *v2=sides[f].v[2];
        float e1[3]={v1[0]-v0[0],v1[1]-v0[1],v1[2]-v0[2]};
        float e2[3]={v2[0]-v0[0],v2[1]-v0[1],v2[2]-v0[2]};
        float n[3]={e1[1]*e2[2]-e1[2]*e2[1],
                    e1[2]*e2[0]-e1[0]*e2[2],
                    e1[0]*e2[1]-e1[1]*e2[0]};
        float len=sqrtf(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
        if (len>0.0001f){n[0]/=len;n[1]/=len;n[2]/=len;}
        for (int t=0;t<2;t++) for (int k=0;k<3;k++) {
            float *vv=sides[f].v[tris[t][k]];
            b[off++]=vv[0]; b[off++]=vv[1]; b[off++]=vv[2];
            b[off++]=n[0];  b[off++]=n[1];  b[off++]=n[2];
        }
    }
    off = add_box(b,off, cx-th,y1-0.01f,cz-th, cx+th,y1,cz+th);
    off = add_box(b,off, cx-bh,y0,cz-bh, cx+bh,y0+0.01f,cz+bh);
    return off;
}
