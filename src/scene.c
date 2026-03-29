#include "scene.h"
#include "config.h"
#include <stdlib.h>

static Mesh build_sea(void) {
    float v[36];
    add_quad_y(v, 0, -600,-600, 600,600, 0.0f);
    return mesh_create(v, 36);
}

static Mesh build_arena(void) {
    float v[216];
    add_box(v, 0, -ARENA_HALF,ARENA_BASE,-ARENA_HALF,
                  ARENA_HALF, ARENA_TOP,  ARENA_HALF);
    return mesh_create(v, 216);
}

static Mesh build_arena_accents(void) {
    float aw=0.4f, ay=ARENA_TOP, ah=0.25f, hs=ARENA_HALF;
    int grid_spacing=8, grid_lines=(int)(2*hs/grid_spacing)-1;
    int total = 4 + grid_lines*2;
    float *v = malloc(total * 216 * sizeof(float));
    int off=0;
    off=add_box(v,off,-hs,ay,-hs,  hs,     ay+ah,-hs+aw);
    off=add_box(v,off,-hs,ay,hs-aw,hs,     ay+ah, hs);
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
        float cap_h=2.5f, cap_hw=6.0f;
        off=add_box(v,off,
            cx-cap_hw, PILLAR_HEIGHT,         cz-cap_hw,
            cx+cap_hw, PILLAR_HEIGHT+cap_h,   cz+cap_hw);
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
    off=add_box(v,off,-0.55f,0.0f, -1.6f,  0.55f,0.65f, 1.6f);  // body
    off=add_box(v,off,-0.30f,0.65f,-1.1f,  0.30f,1.35f, 0.2f);  // cockpit
    off=add_box(v,off,-0.08f,0.3f,  0.6f,  0.08f,1.1f,  1.6f);  // rear fin
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
    return s;
}

void scene_free(Scene *s) {
    mesh_free(&s->sea);
    mesh_free(&s->arena);
    mesh_free(&s->arena_acc);
    mesh_free(&s->pillars);
    mesh_free(&s->pillar_acc);
    mesh_free(&s->bike_mesh);
}