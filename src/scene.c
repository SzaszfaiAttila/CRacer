#include "scene.h"
#include "config.h"
#include <stdlib.h>
#include <math.h>

// ── Water grid: flat NxN quad grid, y=0 — animated in vertex shader ───────────
static Mesh build_sea(void) {
    const int   RES    = 90;
    const float EXTENT = 600.0f;
    const float STEP   = (2.0f * EXTENT) / RES;

    // RES*RES quads × 6 verts × 6 floats
    int n = RES * RES * 6 * 6;
    float *v = malloc(n * sizeof(float));
    int off = 0;
    for (int iz = 0; iz < RES; iz++) {
        for (int ix = 0; ix < RES; ix++) {
            float x0 = -EXTENT + ix * STEP;
            float x1 = x0 + STEP;
            float z0 = -EXTENT + iz * STEP;
            float z1 = z0 + STEP;
            off = add_quad_y(v, off, x0, z0, x1, z1, 0.0f);
        }
    }
    Mesh m = mesh_create(v, off);
    free(v);
    return m;
}

// ── Moon disc: flat circle in the XY plane, normals pointing +Z ───────────────
// Translate to world position in main via model matrix.
static Mesh build_moon(void) {
    const int   SEGS   = 48;
    const float RADIUS = 30.0f;
    // SEGS triangles × 3 verts × 6 floats
    float *v = malloc(SEGS * 3 * 6 * sizeof(float));
    int off = 0;
    for (int i = 0; i < SEGS; i++) {
        float a0 = (float)i       / SEGS * 6.28318530f;
        float a1 = (float)(i + 1) / SEGS * 6.28318530f;
        // Center
        v[off++]=0;             v[off++]=0;             v[off++]=0;
        v[off++]=0;             v[off++]=0;             v[off++]=1;
        // Edge 0
        v[off++]=cosf(a0)*RADIUS; v[off++]=sinf(a0)*RADIUS; v[off++]=0;
        v[off++]=0;               v[off++]=0;               v[off++]=1;
        // Edge 1
        v[off++]=cosf(a1)*RADIUS; v[off++]=sinf(a1)*RADIUS; v[off++]=0;
        v[off++]=0;               v[off++]=0;               v[off++]=1;
    }
    Mesh m = mesh_create(v, off);
    free(v);
    return m;
}

static Mesh build_arena(void) {
    /* Single top-face quad at ARENA_TOP — removes all walls and bottom so
       the water beneath is visible and there are no invisible side faces.  */
    float v[36];
    add_quad_y(v, 0, -ARENA_HALF, -ARENA_HALF,
                    ARENA_HALF,   ARENA_HALF, ARENA_TOP);
    return mesh_create(v, 36);
}

static Mesh build_arena_accents(void) {
    float aw=0.4f, ay=ARENA_TOP, ah=0.25f, hs=ARENA_HALF;
    int grid_spacing=8, grid_lines=(int)(2*hs/grid_spacing)-1;
    int total = 4 + grid_lines*2;
    float *v = malloc(total * 216 * sizeof(float));
    int off=0;
    off=add_box(v,off,-hs,ay,-hs,   hs,    ay+ah,-hs+aw);
    off=add_box(v,off,-hs,ay,hs-aw, hs,    ay+ah, hs);
    off=add_box(v,off,-hs,ay,-hs,  -hs+aw, ay+ah, hs);
    off=add_box(v,off, hs-aw,ay,-hs,hs,    ay+ah, hs);
    float gw=0.10f, gh=0.04f;
    for (int i=1; i*grid_spacing<(int)(2*hs); i++) {
        float pos = -hs + i*grid_spacing;
        off=add_box(v,off,-hs,ay,pos-gw, hs,    ay+gh,pos+gw);
        off=add_box(v,off,pos-gw,ay,-hs, pos+gw,ay+gh,hs);
    }
    Mesh m=mesh_create(v,off); free(v); return m;
}

static Mesh build_pillars(void) {
    static const float PP[4][2] = {
        {-PILLAR_DIST,-PILLAR_DIST},{PILLAR_DIST,-PILLAR_DIST},
        {-PILLAR_DIST, PILLAR_DIST},{PILLAR_DIST, PILLAR_DIST}
    };
    float *v=malloc(4*5000*sizeof(float)); int off=0;
    for (int i=0;i<4;i++) {
        float cx=PP[i][0], cz=PP[i][1];
        off=add_frustum(v,off,cx,cz,PILLAR_BH,PILLAR_TH,ARENA_BASE,PILLAR_HEIGHT);
        float cap_h=2.5f, cap_hw=3.5f;
        off=add_box(v,off,
            cx-cap_hw, PILLAR_HEIGHT,        cz-cap_hw,
            cx+cap_hw, PILLAR_HEIGHT+cap_h,  cz+cap_hw);
    }
    Mesh m=mesh_create(v,off); free(v); return m;
}

static Mesh build_pillar_accents(void) {
    static const float PP[4][2] = {
        {-PILLAR_DIST,-PILLAR_DIST},{PILLAR_DIST,-PILLAR_DIST},
        {-PILLAR_DIST, PILLAR_DIST},{PILLAR_DIST, PILLAR_DIST}
    };
    float *v=malloc(4*8*216*sizeof(float)); int off=0;
    for (int i=0;i<4;i++) {
        float cx=PP[i][0],cz=PP[i][1];
        float th=PILLAR_TH, yt=PILLAR_HEIGHT, aw=0.25f, ah=0.4f;
        off=add_box(v,off,cx-th-aw,yt-ah,cz-th-aw,cx+th+aw,yt,cz-th);
        off=add_box(v,off,cx-th-aw,yt-ah,cz+th,   cx+th+aw,yt,cz+th+aw);
        off=add_box(v,off,cx-th-aw,yt-ah,cz-th,   cx-th,   yt,cz+th);
        off=add_box(v,off,cx+th,   yt-ah,cz-th,   cx+th+aw,yt,cz+th);
        float ym=PILLAR_HEIGHT*0.5f, bm=PILLAR_BH+(PILLAR_TH-PILLAR_BH)*0.5f;
        off=add_box(v,off,cx-bm-aw,ym-ah*0.5f,cz-bm-aw,cx+bm+aw,ym,cz-bm);
        off=add_box(v,off,cx-bm-aw,ym-ah*0.5f,cz+bm,   cx+bm+aw,ym,cz+bm+aw);
        off=add_box(v,off,cx-bm-aw,ym-ah*0.5f,cz-bm,   cx-bm,   ym,cz+bm);
        off=add_box(v,off,cx+bm,   ym-ah*0.5f,cz-bm,   cx+bm+aw,ym,cz+bm);
    }
    Mesh m=mesh_create(v,off); free(v); return m;
}

static Mesh build_bike_mesh(void) {
    float v[216*3]; int off=0;
    off=add_box(v,off,-0.55f,0.0f,-1.6f,  0.55f,0.65f, 1.6f);
    off=add_box(v,off,-0.30f,0.65f,-1.1f, 0.30f,1.35f, 0.2f);
    off=add_box(v,off,-0.08f,0.3f,  0.6f, 0.08f,1.1f,  1.6f);
    return mesh_create(v,off);
}

Scene scene_build(void) {
    Scene s;
    s.sea        = build_sea();
    s.arena      = build_arena();
    s.arena_acc  = build_arena_accents();
    s.pillars    = build_pillars();
    s.pillar_acc = build_pillar_accents();
    s.bike_mesh  = build_bike_mesh();
    s.moon       = build_moon();
    return s;
}

void scene_free(Scene *s) {
    mesh_free(&s->sea);
    mesh_free(&s->arena);
    mesh_free(&s->arena_acc);
    mesh_free(&s->pillars);
    mesh_free(&s->pillar_acc);
    mesh_free(&s->bike_mesh);
    mesh_free(&s->moon);
}
