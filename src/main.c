#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "shader.h"
#include "camera.h"

#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720
#define WINDOW_TITLE  "CRacer — TRON"

// ═══════════════════════════════════════════════════════════════ ARENA PARAMS
#define ARENA_HALF   48.0f
#define ARENA_TOP     3.5f
#define ARENA_BASE   -4.0f
#define PILLAR_DIST  80.0f
#define PILLAR_BH     5.0f
#define PILLAR_TH     2.2f
#define PILLAR_HEIGHT 28.0f

// ═══════════════════════════════════════════════════════════════════ BIKE / TRAIL
#define BIKE_SPEED      14.0f
#define TRAIL_HEIGHT     1.6f   // just slightly taller than the bike (1.35)
#define TRAIL_W          0.22f
#define TRAIL_LENGTH    55.0f   // how many world-units of trail are visible at once
#define COLLISION_R      1.1f
#define MAX_WAYPOINTS  4096

// ══════════════════════════════════════════════════════════════════ PARTICLES
#define NUM_PILLARS       4
#define PARTS_PER_PILLAR  140
#define TOTAL_PARTS       (NUM_PILLARS * PARTS_PER_PILLAR)
// Each particle billboard = 6 verts × 7 floats (xyz + rgba)
#define PART_FLOATS_PER   (6 * 7)
#define PART_BUF_FLOATS   (TOTAL_PARTS * PART_FLOATS_PER)

typedef struct {
    float x,  y,  z;    // world position
    float vx, vy, vz;   // velocity
    float life;          // remaining life [0..1]
    float max_life;      // total lifespan in seconds
    float size;          // billboard half-size
    int   pillar;        // which pillar (0-3)
} Particle;

static Particle particles[TOTAL_PARTS];
static float    part_buf[PART_BUF_FLOATS];

// Pillar top centres (same order as PILLAR_POS in scene builders)
static const float PTOP[4][2] = {
    {-PILLAR_DIST, -PILLAR_DIST},
    { PILLAR_DIST, -PILLAR_DIST},
    {-PILLAR_DIST,  PILLAR_DIST},
    { PILLAR_DIST,  PILLAR_DIST},
};

// Minimal LCG for particle randomness (seeded per-particle, no global state)
static float lcg_rand(unsigned int *s){
    *s = (*s) * 1664525u + 1013904223u;
    return (float)(*s & 0x7FFFFFFF) / (float)0x7FFFFFFF;
}

static void particle_spawn(Particle *p, int pillar_idx, unsigned int *seed) {
    float r = lcg_rand(seed), theta = lcg_rand(seed) * 6.2832f;
    r = r * 2.0f; // spawn radius around pillar top
    p->x       = PTOP[pillar_idx][0] + r * cosf(theta);
    p->y       = PILLAR_HEIGHT + 0.3f;
    p->z       = PTOP[pillar_idx][1] + r * sinf(theta);
    p->vx      = (lcg_rand(seed) - 0.5f) * 1.8f;
    p->vy      = 3.5f + lcg_rand(seed) * 7.0f;
    p->vz      = (lcg_rand(seed) - 0.5f) * 1.8f;
    p->max_life= 0.9f + lcg_rand(seed) * 1.4f;
    p->life    = p->max_life * lcg_rand(seed); // stagger initial phase
    p->size    = 1.4f + lcg_rand(seed) * 1.2f;
    p->pillar  = pillar_idx;
}

static void particles_init(void) {
    // Each pillar gets its own seed so they're statistically identical
    static const unsigned int pillar_seeds[4] = {
        0xDEADBEEF, 0xCAFEBABE, 0x12345678, 0xABCDEF01
    };
    for (int i = 0; i < TOTAL_PARTS; i++) {
        int pillar = i / PARTS_PER_PILLAR;
        unsigned int seed = pillar_seeds[pillar] + (unsigned int)(i % PARTS_PER_PILLAR) * 6271u;
        particle_spawn(&particles[i], pillar, &seed);
    }
}

static void particles_update(float dt) {
    unsigned int seed = 0xCAFEBABE;
    float t = (float)glfwGetTime();
    for (int i = 0; i < TOTAL_PARTS; i++) {
        Particle *p = &particles[i];
        p->life -= dt;
        if (p->life <= 0.0f) {
            particle_spawn(p, p->pillar, &seed);
            seed += i * 6271u;
            continue;
        }
        // Turbulence: perturb horizontal velocity with time-varying noise
        float noise = sinf(t * 3.1f + i * 0.37f);
        p->vx += noise * 0.9f * dt;
        p->vz += cosf(t * 2.7f + i * 0.53f) * 0.9f * dt;
        // Drag on horizontal
        p->vx *= (1.0f - 1.5f * dt);
        p->vz *= (1.0f - 1.5f * dt);
        // Slight vertical decel at top
        p->vy -= 0.4f * dt;
        if (p->vy < 0.5f) p->vy = 0.5f;
        p->x += p->vx * dt;
        p->y += p->vy * dt;
        p->z += p->vz * dt;
    }
}

// Build billboard quads for all particles into part_buf
// Returns number of floats written
static int particles_build(vec3 cam_right, vec3 cam_up) {
    int off = 0;
    for (int i = 0; i < TOTAL_PARTS; i++) {
        Particle *p = &particles[i];
        float ratio = p->life / p->max_life; // 1 = just born, 0 = dying

        // Color: born = bright blue-white, mid = cyan-blue, dying = deep blue → transparent
        float r, g, b, a;
        if (ratio > 0.65f) {
            float t = (ratio - 0.65f) / 0.35f;
            r = 0.5f + t * 0.5f;   // 0.5 → 1.0
            g = 0.7f + t * 0.3f;   // 0.7 → 1.0
            b = 1.0f;
        } else if (ratio > 0.25f) {
            float t = (ratio - 0.25f) / 0.40f;
            r = 0.05f + t * 0.45f; // 0.05 → 0.5
            g = 0.25f + t * 0.45f; // 0.25 → 0.7
            b = 1.0f;
        } else {
            float t = ratio / 0.25f;
            r = 0.0f;
            g = t * 0.1f;
            b = 0.4f + t * 0.6f;
        }
        a = ratio * 0.85f;

        float hs = p->size * (0.4f + ratio * 0.6f); // shrink as it dies

        // Billboard corners
        float rx = cam_right[0]*hs, ry = cam_right[1]*hs, rz = cam_right[2]*hs;
        float ux = cam_up[0]*hs,    uy = cam_up[1]*hs,    uz = cam_up[2]*hs;

        float cx=p->x, cy=p->y, cz=p->z;
        float v[4][3] = {
            {cx - rx - ux, cy - ry - uy, cz - rz - uz}, // BL
            {cx + rx - ux, cy + ry - uy, cz + rz - uz}, // BR
            {cx + rx + ux, cy + ry + uy, cz + rz + uz}, // TR
            {cx - rx + ux, cy - ry + uy, cz - rz + uz}, // TL
        };
        int idx[6] = {0,1,2, 0,2,3};
        for (int k = 0; k < 6; k++) {
            float *vv = v[idx[k]];
            part_buf[off++] = vv[0];
            part_buf[off++] = vv[1];
            part_buf[off++] = vv[2];
            part_buf[off++] = r;
            part_buf[off++] = g;
            part_buf[off++] = b;
            part_buf[off++] = a;
        }
    }
    return off;
}

typedef enum { DIR_NORTH=0, DIR_EAST=1, DIR_SOUTH=2, DIR_WEST=3 } Direction;

// Per-direction movement deltas and Y-rotation (radians, bike faces -Z at 0)
static const float DDX[4]       = {  0.0f,  1.0f, 0.0f, -1.0f };
static const float DDZ[4]       = { -1.0f,  0.0f, 1.0f,  0.0f };
static const float DANGLE[4]    = {  0.0f, -GLM_PIf/2.0f, GLM_PIf, GLM_PIf/2.0f };

typedef struct {
    float     x, z;
    Direction dir;
    int       alive;
    float     wp_x[MAX_WAYPOINTS];
    float     wp_z[MAX_WAYPOINTS];
    float     wp_dist[MAX_WAYPOINTS]; // cumulative distance from start at each waypoint
    int       wp_count;
    float     total_dist;             // total distance the head has traveled
} Bike;

static void bike_reset(Bike *b) {
    b->x          = 0.0f;
    b->z          = 15.0f;
    b->dir        = DIR_NORTH;
    b->alive      = 1;
    b->wp_x[0]   = b->x;
    b->wp_z[0]   = b->z;
    b->wp_dist[0] = 0.0f;
    b->wp_count   = 1;
    b->total_dist = 0.0f;
}

static void bike_turn_left(Bike *b) {
    if (!b->alive) return;
    b->wp_x[b->wp_count]    = b->x;
    b->wp_z[b->wp_count]    = b->z;
    b->wp_dist[b->wp_count] = b->total_dist;
    b->wp_count++;
    b->dir = (Direction)((b->dir + 3) % 4);
}

static void bike_turn_right(Bike *b) {
    if (!b->alive) return;
    b->wp_x[b->wp_count]    = b->x;
    b->wp_z[b->wp_count]    = b->z;
    b->wp_dist[b->wp_count] = b->total_dist;
    b->wp_count++;
    b->dir = (Direction)((b->dir + 1) % 4);
}

// Check if point (px,pz) with radius r hits an axis-aligned trail segment
static int seg_hit(float px, float pz, float r,
                   float ax, float az, float bx, float bz) {
    float hw = TRAIL_W / 2.0f + r;
    if (fabsf(ax - bx) < 0.001f) {   // N/S segment (constant X)
        float mn = fminf(az, bz), mx = fmaxf(az, bz);
        return fabsf(px - ax) < hw && pz > mn - r && pz < mx + r;
    } else {                           // E/W segment (constant Z)
        float mn = fminf(ax, bx), mx = fmaxf(ax, bx);
        return fabsf(pz - az) < hw && px > mn - r && px < mx + r;
    }
}

// How many units behind the head are non-collidable (prevents instant self-hit on spawn/turn)
#define GRACE_DIST 5.0f

static int bike_check_collision(Bike *b) {
    float px = b->x, pz = b->z;
    // Arena boundary
    if (fabsf(px) > ARENA_HALF - COLLISION_R || fabsf(pz) > ARENA_HALF - COLLISION_R)
        return 1;

    float tail_dist  = b->total_dist - TRAIL_LENGTH;
    float grace_dist = b->total_dist - GRACE_DIST;

    // Mirror exact point list from build_trail
    int total_pts = b->wp_count + 1;
    float pts_x[MAX_WAYPOINTS+1], pts_z[MAX_WAYPOINTS+1], pts_d[MAX_WAYPOINTS+1];
    for (int i = 0; i < b->wp_count; i++) {
        pts_x[i] = b->wp_x[i]; pts_z[i] = b->wp_z[i]; pts_d[i] = b->wp_dist[i];
    }
    pts_x[b->wp_count] = b->x;
    pts_z[b->wp_count] = b->z;
    pts_d[b->wp_count] = b->total_dist;

    for (int i = 0; i + 1 < total_pts; i++) {
        float sd = pts_d[i], ed = pts_d[i+1];
        if (ed   < tail_dist)  continue; // fully faded, no collision
        if (sd   > grace_dist) continue; // within grace zone near head, skip

        // Clip segment to [tail_dist, grace_dist] — exactly what is visible & solid
        float cs = (tail_dist  > sd) ? tail_dist  : sd;
        float ce = (grace_dist < ed) ? grace_dist : ed;
        if (cs >= ce) continue;

        float ax = pts_x[i], az = pts_z[i], bx = pts_x[i+1], bz = pts_z[i+1];
        float len = ed - sd;
        if (len < 0.0001f) continue;
        float ts = (cs - sd) / len, te = (ce - sd) / len;
        float cax = ax+(bx-ax)*ts, caz = az+(bz-az)*ts;
        float cbx = ax+(bx-ax)*te, cbz = az+(bz-az)*te;

        if (seg_hit(px, pz, COLLISION_R, cax, caz, cbx, cbz))
            return 1;
    }
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════ MESH
typedef struct { GLuint vao, vbo; int vert_count; } Mesh;
typedef struct { GLuint vao, vbo; int vert_count; int max_floats; } DynMesh;

// Particle mesh: pos(3) + color(4) = 7 floats per vertex, no normals
typedef struct { GLuint vao, vbo; int vert_count; } PMesh;

static PMesh pmesh_create(int max_floats) {
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

static void pmesh_update(PMesh *m, float *data, int float_count) {
    m->vert_count = float_count / 7;
    glBindBuffer(GL_ARRAY_BUFFER, m->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, float_count * sizeof(float), data);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void pmesh_draw(PMesh *m) {
    if (!m->vert_count) return;
    glBindVertexArray(m->vao);
    glDrawArrays(GL_TRIANGLES, 0, m->vert_count);
    glBindVertexArray(0);
}

static void pmesh_free(PMesh *m) {
    glDeleteVertexArrays(1, &m->vao); glDeleteBuffers(1, &m->vbo);
}

static Mesh mesh_create(float *v, int n) {
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

static DynMesh dynmesh_create(int max_floats) {
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

static void dynmesh_update(DynMesh *m, float *data, int float_count) {
    m->vert_count = float_count / 6;
    glBindBuffer(GL_ARRAY_BUFFER, m->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, float_count * sizeof(float), data);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void mesh_draw(Mesh *m)    { glBindVertexArray(m->vao); glDrawArrays(GL_TRIANGLES, 0, m->vert_count); }
static void dynmesh_draw(DynMesh *m) {
    if (m->vert_count == 0) return;
    glBindVertexArray(m->vao); glDrawArrays(GL_TRIANGLES, 0, m->vert_count);
}
static void mesh_free(Mesh *m)    { glDeleteVertexArrays(1,&m->vao); glDeleteBuffers(1,&m->vbo); }
static void dynmesh_free(DynMesh *m) { glDeleteVertexArrays(1,&m->vao); glDeleteBuffers(1,&m->vbo); }

// ════════════════════════════════════════════════════════════ GEOMETRY BUILDERS
static int add_quad_y(float *b, int off,
                      float x0, float z0, float x1, float z1, float y) {
    float q[6][6] = {
        {x0,y,z0,0,1,0},{x1,y,z0,0,1,0},{x0,y,z1,0,1,0},
        {x1,y,z0,0,1,0},{x1,y,z1,0,1,0},{x0,y,z1,0,1,0},
    };
    for (int i=0;i<6;i++) for(int j=0;j<6;j++) b[off++]=q[i][j];
    return off;
}

static int add_box(float *b, int off,
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
    for(int f=0;f<6;f++) for(int t=0;t<2;t++) for(int k=0;k<3;k++) {
        float *vv=faces[f].v[tris[t][k]];
        b[off++]=vv[0];b[off++]=vv[1];b[off++]=vv[2];
        b[off++]=faces[f].n[0];b[off++]=faces[f].n[1];b[off++]=faces[f].n[2];
    }
    return off;
}

static int add_frustum(float *b, int off,
                       float cx, float cz, float bh, float th,
                       float y0, float y1) {
    typedef struct { float v[4][3]; } SFace;
    SFace sides[4] = {
        {{{cx+bh,y0,cz-bh},{cx+bh,y0,cz+bh},{cx+th,y1,cz+th},{cx+th,y1,cz-th}}},
        {{{cx-bh,y0,cz+bh},{cx-bh,y0,cz-bh},{cx-th,y1,cz-th},{cx-th,y1,cz+th}}},
        {{{cx+bh,y0,cz+bh},{cx-bh,y0,cz+bh},{cx-th,y1,cz+th},{cx+th,y1,cz+th}}},
        {{{cx-bh,y0,cz-bh},{cx+bh,y0,cz-bh},{cx+th,y1,cz-th},{cx-th,y1,cz-th}}},
    };
    int tris[2][3]={{0,1,2},{0,2,3}};
    for(int f=0;f<4;f++) {
        float *v0=sides[f].v[0],*v1=sides[f].v[1],*v2=sides[f].v[2];
        float e1[3]={v1[0]-v0[0],v1[1]-v0[1],v1[2]-v0[2]};
        float e2[3]={v2[0]-v0[0],v2[1]-v0[1],v2[2]-v0[2]};
        float n[3]={e1[1]*e2[2]-e1[2]*e2[1],e1[2]*e2[0]-e1[0]*e2[2],e1[0]*e2[1]-e1[1]*e2[0]};
        float len=sqrtf(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
        if(len>0.0001f){n[0]/=len;n[1]/=len;n[2]/=len;}
        for(int t=0;t<2;t++) for(int k=0;k<3;k++) {
            float *vv=sides[f].v[tris[t][k]];
            b[off++]=vv[0];b[off++]=vv[1];b[off++]=vv[2];
            b[off++]=n[0];b[off++]=n[1];b[off++]=n[2];
        }
    }
    off = add_box(b,off, cx-th,y1-0.01f,cz-th, cx+th,y1,cz+th);
    off = add_box(b,off, cx-bh,y0,cz-bh, cx+bh,y0+0.01f,cz+bh);
    return off;
}

// ══════════════════════════════════════════════════════════ SCENE MESH BUILDERS
static Mesh build_sea(void) {
    float v[36]; add_quad_y(v,0,-600,-600,600,600,0);
    return mesh_create(v,36);
}

static Mesh build_arena(void) {
    float v[216];
    add_box(v,0,-ARENA_HALF,ARENA_BASE,-ARENA_HALF,ARENA_HALF,ARENA_TOP,ARENA_HALF);
    return mesh_create(v,216);
}

static Mesh build_arena_accents(void) {
    float aw=0.4f, ay=ARENA_TOP, ah=0.25f, hs=ARENA_HALF;
    int grid_spacing=8, grid_lines=(int)(2*hs/grid_spacing)-1;
    int total = 4 + grid_lines*2;
    float *v = malloc(total*216*sizeof(float));
    int off=0;
    off=add_box(v,off,-hs,ay,-hs,hs,ay+ah,-hs+aw);
    off=add_box(v,off,-hs,ay,hs-aw,hs,ay+ah,hs);
    off=add_box(v,off,-hs,ay,-hs,-hs+aw,ay+ah,hs);
    off=add_box(v,off,hs-aw,ay,-hs,hs,ay+ah,hs);
    float gw=0.10f,gh=0.04f;
    for(int i=1;i*grid_spacing<(int)(2*hs);i++){
        float pos=-hs+i*grid_spacing;
        off=add_box(v,off,-hs,ay,pos-gw,hs,ay+gh,pos+gw);
        off=add_box(v,off,pos-gw,ay,-hs,pos+gw,ay+gh,hs);
    }
    Mesh m=mesh_create(v,off); free(v); return m;
}

static Mesh build_pillars(void) {
    static const float PP[4][2]={{-PILLAR_DIST,-PILLAR_DIST},{PILLAR_DIST,-PILLAR_DIST},
                                  {-PILLAR_DIST,PILLAR_DIST},{PILLAR_DIST,PILLAR_DIST}};
    float *v=malloc(4*4000*sizeof(float)); int off=0;
    for(int i=0;i<4;i++)
        off=add_frustum(v,off,PP[i][0],PP[i][1],PILLAR_BH,PILLAR_TH,ARENA_BASE,PILLAR_HEIGHT);
    Mesh m=mesh_create(v,off); free(v); return m;
}

static Mesh build_pillar_accents(void) {
    static const float PP[4][2]={{-PILLAR_DIST,-PILLAR_DIST},{PILLAR_DIST,-PILLAR_DIST},
                                  {-PILLAR_DIST,PILLAR_DIST},{PILLAR_DIST,PILLAR_DIST}};
    float *v=malloc(4*8*216*sizeof(float)); int off=0;
    for(int i=0;i<4;i++){
        float cx=PP[i][0],cz=PP[i][1],th=PILLAR_TH,yt=PILLAR_HEIGHT,aw=0.25f,ah=0.4f;
        off=add_box(v,off,cx-th-aw,yt-ah,cz-th-aw,cx+th+aw,yt,cz-th);
        off=add_box(v,off,cx-th-aw,yt-ah,cz+th,   cx+th+aw,yt,cz+th+aw);
        off=add_box(v,off,cx-th-aw,yt-ah,cz-th,   cx-th,   yt,cz+th);
        off=add_box(v,off,cx+th,   yt-ah,cz-th,   cx+th+aw,yt,cz+th);
        float ym=PILLAR_HEIGHT*0.5f,bm=PILLAR_BH+(PILLAR_TH-PILLAR_BH)*0.5f;
        off=add_box(v,off,cx-bm-aw,ym-ah*0.5f,cz-bm-aw,cx+bm+aw,ym,cz-bm);
        off=add_box(v,off,cx-bm-aw,ym-ah*0.5f,cz+bm,   cx+bm+aw,ym,cz+bm+aw);
        off=add_box(v,off,cx-bm-aw,ym-ah*0.5f,cz-bm,   cx-bm,   ym,cz+bm);
        off=add_box(v,off,cx+bm,   ym-ah*0.5f,cz-bm,   cx+bm+aw,ym,cz+bm);
    }
    Mesh m=mesh_create(v,off); free(v); return m;
}

// Bike mesh: elongated body + cockpit fin, facing -Z in model space
static Mesh build_bike(void) {
    float v[216*3]; int off=0;
    // Body
    off=add_box(v,off,-0.55f,0.0f,-1.6f, 0.55f,0.65f,1.6f);
    // Cockpit
    off=add_box(v,off,-0.30f,0.65f,-1.1f, 0.30f,1.35f,0.2f);
    // Rear fin
    off=add_box(v,off,-0.08f,0.3f,0.6f,  0.08f,1.1f, 1.6f);
    return mesh_create(v,off);
}

// ══════════════════════════════════════════════════════ TRAIL GEOMETRY BUILDER
// 216 floats per box, one box per segment
#define TRAIL_BUF_FLOATS (MAX_WAYPOINTS * 216)
static float trail_buf[TRAIL_BUF_FLOATS];

static int build_trail(Bike *b) {
    int off = 0;
    float y0 = ARENA_TOP, y1 = ARENA_TOP + TRAIL_HEIGHT;
    float hw = TRAIL_W / 2.0f;

    float tail_dist = b->total_dist - TRAIL_LENGTH;

    // Full point list: waypoints + current head
    int total_pts = b->wp_count + 1;
    float pts_x[MAX_WAYPOINTS+1], pts_z[MAX_WAYPOINTS+1];
    float pts_d[MAX_WAYPOINTS+1];
    for (int i = 0; i < b->wp_count; i++) {
        pts_x[i] = b->wp_x[i];
        pts_z[i] = b->wp_z[i];
        pts_d[i] = b->wp_dist[i];
    }
    pts_x[b->wp_count] = b->x;
    pts_z[b->wp_count] = b->z;
    pts_d[b->wp_count] = b->total_dist;

    for (int i = 0; i + 1 < total_pts; i++) {
        float seg_end_dist = pts_d[i+1];
        if (seg_end_dist < tail_dist) continue; // entire segment faded

        float ax = pts_x[i], az = pts_z[i];
        float bx = pts_x[i+1], bz = pts_z[i+1];

        // Clip start of segment to tail position
        float seg_start_dist = pts_d[i];
        if (tail_dist > seg_start_dist) {
            float t = (tail_dist - seg_start_dist) / (seg_end_dist - seg_start_dist);
            ax = ax + (bx - ax) * t;
            az = az + (bz - az) * t;
        }

        if (fabsf(pts_x[i] - bx) < 0.001f) // N/S segment
            off = add_box(trail_buf, off, ax-hw, y0, fminf(az,bz), ax+hw, y1, fmaxf(az,bz));
        else                                 // E/W segment
            off = add_box(trail_buf, off, fminf(ax,bx), y0, az-hw, fmaxf(ax,bx), y1, az+hw);
    }
    return off;
}

// ══════════════════════════════════════════════════════════════ SHADER HELPERS
static void obj(GLuint sh, float r,float g,float b) {
    shader_set_vec3(sh,"objectColor",r,g,b);
    shader_set_vec3(sh,"emissive",0,0,0);
    shader_set_float(sh,"ambientStr",0.12f);
}
static void emissive(GLuint sh, float or,float og,float ob,float er,float eg,float eb) {
    shader_set_vec3(sh,"objectColor",or,og,ob);
    shader_set_vec3(sh,"emissive",er,eg,eb);
    shader_set_float(sh,"ambientStr",0.05f);
}

// ═══════════════════════════════════════════════════════════════════════ MAIN
typedef enum { CAM_FOLLOW=0, CAM_TOPDOWN=1, CAM_FREE=2 } CamMode;

Camera  camera;
float   last_x=WINDOW_WIDTH/2.0f, last_y=WINDOW_HEIGHT/2.0f;
int     first_mouse=1;
float   delta_time=0,last_frame=0;
CamMode cam_mode=CAM_FOLLOW;
int     show_help=0;

void framebuffer_size_callback(GLFWwindow*w,int W,int H){(void)w;glViewport(0,0,W,H);}

void mouse_callback(GLFWwindow*w,double xpos,double ypos){
    (void)w;
    if(first_mouse){last_x=xpos;last_y=ypos;first_mouse=0;}
    float xo=(float)(xpos-last_x), yo=-(float)(ypos-last_y);
    last_x=xpos;last_y=ypos;
    if(cam_mode==CAM_FREE) camera_process_mouse(&camera,xo,yo);
}

int main(void) {
    if(!glfwInit()){fprintf(stderr,"GLFW fail\n");return -1;}
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,6);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *win=glfwCreateWindow(WINDOW_WIDTH,WINDOW_HEIGHT,WINDOW_TITLE,NULL,NULL);
    if(!win){fprintf(stderr,"Window fail\n");glfwTerminate();return -1;}
    glfwMakeContextCurrent(win);
    glfwSetFramebufferSizeCallback(win,framebuffer_size_callback);
    glfwSetCursorPosCallback(win,mouse_callback);
    glfwSetInputMode(win,GLFW_CURSOR,GLFW_CURSOR_DISABLED);

    if(!gladLoadGL(glfwGetProcAddress)){fprintf(stderr,"GLAD fail\n");return -1;}
    glEnable(GL_DEPTH_TEST);
    printf("OpenGL %s\n",glGetString(GL_VERSION));
    printf("F1 — toggle controls help\n");

    camera_init(&camera,0.0f,70.0f,130.0f);

    GLuint sh  = shader_create("../shaders/vertex.glsl",       "../shaders/fragment.glsl");
    GLuint psh = shader_create("../shaders/particle_vert.glsl", "../shaders/particle_frag.glsl");

    particles_init();

    // Scene
    Mesh sea            = build_sea();
    Mesh arena          = build_arena();
    Mesh arena_acc      = build_arena_accents();
    Mesh pillars        = build_pillars();
    Mesh pillar_acc     = build_pillar_accents();
    Mesh bike_mesh      = build_bike();
    DynMesh trail_mesh  = dynmesh_create(TRAIL_BUF_FLOATS);
    PMesh   part_mesh   = pmesh_create(PART_BUF_FLOATS);

    // Bike state
    Bike bike;
    bike_reset(&bike);

    // Key state (single-press tracking)
    int prev_a=0,prev_d=0,prev_tab=0,prev_c=0,prev_r=0,prev_f1=0;
    int dead_announced=0;

    // Smooth follow-cam position
    float cam_px=0,cam_py=70,cam_pz=130;

    while(!glfwWindowShouldClose(win)){
        float now=(float)glfwGetTime();
        delta_time=now-last_frame; last_frame=now;

        // ── Input ──────────────────────────────────────────────────
        if(glfwGetKey(win,GLFW_KEY_ESCAPE)==GLFW_PRESS)
            glfwSetWindowShouldClose(win,1);

        int cur_a   = glfwGetKey(win,GLFW_KEY_A)       ==GLFW_PRESS;
        int cur_d   = glfwGetKey(win,GLFW_KEY_D)       ==GLFW_PRESS;
        int cur_tab = glfwGetKey(win,GLFW_KEY_TAB)     ==GLFW_PRESS;
        int cur_c   = glfwGetKey(win,GLFW_KEY_C)       ==GLFW_PRESS;
        int cur_r   = glfwGetKey(win,GLFW_KEY_R)       ==GLFW_PRESS;
        int cur_f1  = glfwGetKey(win,GLFW_KEY_F1)      ==GLFW_PRESS;

        // Bike turns only when not in free-cam
        if(cam_mode != CAM_FREE){
            if(cur_a && !prev_a) bike_turn_left(&bike);
            if(cur_d && !prev_d) bike_turn_right(&bike);
        }

        // Camera mode switches
        if(cur_tab && !prev_tab){
            cam_mode = (cam_mode == CAM_FREE) ? CAM_FOLLOW : CAM_FREE;
            first_mouse=1;
        }
        if(cur_c && !prev_c){
            cam_mode = (cam_mode == CAM_TOPDOWN) ? CAM_FOLLOW : CAM_TOPDOWN;
        }

        if(cur_r  && !prev_r)  { bike_reset(&bike); dead_announced=0; }
        if(cur_f1 && !prev_f1) {
            show_help = !show_help;
            if(show_help){
                printf("\n╔══════════════════════════════╗\n");
                printf("║       CRACER  CONTROLS       ║\n");
                printf("╠══════════════════════════════╣\n");
                printf("║  A / D       Turn bike       ║\n");
                printf("║  R           Restart         ║\n");
                printf("║  C           Top-down view   ║\n");
                printf("║  Tab         Free camera     ║\n");
                printf("║  (Free cam)  WASD + mouse    ║\n");
                printf("║  Space/Ctrl  Free cam up/dn  ║\n");
                printf("║  F1          Toggle this help║\n");
                printf("║  Esc         Quit            ║\n");
                printf("╚══════════════════════════════╝\n\n");
            }
        }

        prev_a=cur_a; prev_d=cur_d; prev_tab=cur_tab;
        prev_c=cur_c; prev_r=cur_r; prev_f1=cur_f1;

        // Free-cam WASD (arrow keys so A/D still steer bike in free-cam-less modes,
        // but in free-cam mode we detach A/D from bike so use WASD freely)
        if(cam_mode==CAM_FREE){
            if(glfwGetKey(win,GLFW_KEY_W)           ==GLFW_PRESS) camera_process_keyboard(&camera,CAM_FORWARD,  delta_time);
            if(glfwGetKey(win,GLFW_KEY_S)           ==GLFW_PRESS) camera_process_keyboard(&camera,CAM_BACKWARD, delta_time);
            if(glfwGetKey(win,GLFW_KEY_A)           ==GLFW_PRESS) camera_process_keyboard(&camera,CAM_LEFT,     delta_time);
            if(glfwGetKey(win,GLFW_KEY_D)           ==GLFW_PRESS) camera_process_keyboard(&camera,CAM_RIGHT,    delta_time);
            if(glfwGetKey(win,GLFW_KEY_SPACE)       ==GLFW_PRESS) camera_process_keyboard(&camera,CAM_UP,       delta_time);
            if(glfwGetKey(win,GLFW_KEY_LEFT_CONTROL)==GLFW_PRESS) camera_process_keyboard(&camera,CAM_DOWN,     delta_time);
        }

        // ── Bike update ────────────────────────────────────────────
        if(bike.alive){
            bike.x          += DDX[bike.dir] * BIKE_SPEED * delta_time;
            bike.z          += DDZ[bike.dir] * BIKE_SPEED * delta_time;
            bike.total_dist += BIKE_SPEED * delta_time;

            if(bike_check_collision(&bike)){
                bike.alive=0;
                if(!dead_announced){ printf("DEAD! Press R to restart.\n"); dead_announced=1; }
            }
        }

        // ── Trail mesh ─────────────────────────────────────────────
        int trail_floats = build_trail(&bike);
        dynmesh_update(&trail_mesh, trail_buf, trail_floats);

        // ── Particles ──────────────────────────────────────────────
        particles_update(delta_time);

        // ── Camera update ──────────────────────────────────────────
        if(cam_mode==CAM_FOLLOW){
            float dist=22.0f, height=12.0f, ahead=6.0f;
            float tx = bike.x - DDX[bike.dir]*dist;
            float ty = ARENA_TOP + height;
            float tz = bike.z - DDZ[bike.dir]*dist;
            float lp = 1.0f - expf(-8.0f * delta_time);
            cam_px += (tx-cam_px)*lp;
            cam_py += (ty-cam_py)*lp;
            cam_pz += (tz-cam_pz)*lp;
            camera.position[0]=cam_px;
            camera.position[1]=cam_py;
            camera.position[2]=cam_pz;
            vec3 look={bike.x+DDX[bike.dir]*ahead, ARENA_TOP+1.5f, bike.z+DDZ[bike.dir]*ahead};
            glm_vec3_sub(look,camera.position,camera.front);
            glm_normalize(camera.front);
            camera.up[0]=0;camera.up[1]=1;camera.up[2]=0;
        } else if(cam_mode==CAM_TOPDOWN){
            // Fixed overhead: centered on arena, looking straight down
            camera.position[0]=0.0f;
            camera.position[1]=120.0f;
            camera.position[2]=0.0f;
            camera.front[0]=0.0f; camera.front[1]=-1.0f; camera.front[2]=-0.001f;
            glm_normalize(camera.front);
            camera.up[0]=0;camera.up[1]=0;camera.up[2]=-1;
        }
        // CAM_FREE: camera updated directly via keyboard/mouse above

        // ── Render ─────────────────────────────────────────────────
        glClearColor(0.01f,0.01f,0.04f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        shader_use(sh);

        mat4 view,proj,model;
        camera_get_view(&camera,view);
        camera_get_projection(&camera,proj,(float)WINDOW_WIDTH/WINDOW_HEIGHT);
        glm_mat4_identity(model);

        shader_set_mat4(sh,"view",(float*)view);
        shader_set_mat4(sh,"projection",(float*)proj);
        shader_set_mat4(sh,"model",(float*)model);
        shader_set_vec3(sh,"lightPos",  0.0f,300.0f,0.0f);
        shader_set_vec3(sh,"lightColor",0.6f,0.75f, 1.0f);
        shader_set_vec3(sh,"viewPos",
            camera.position[0],camera.position[1],camera.position[2]);

        // Scene objects
        obj(sh,0.02f,0.04f,0.12f); mesh_draw(&sea);
        obj(sh,0.07f,0.07f,0.10f); mesh_draw(&arena);
        emissive(sh,0.2f,0.5f,0.5f,0.0f,0.10f,0.15f); mesh_draw(&arena_acc);
        obj(sh,0.10f,0.10f,0.14f); mesh_draw(&pillars);
        emissive(sh,0.9f,0.95f,1.0f,0.2f,0.25f,0.35f); mesh_draw(&pillar_acc);

        // Trail — white glow
        emissive(sh,1.0f,1.0f,1.0f,0.3f,0.3f,0.3f); dynmesh_draw(&trail_mesh);

        // Bike — set model matrix with position + rotation
        glm_mat4_identity(model);
        vec3 bike_pos = {bike.x, ARENA_TOP, bike.z};
        glm_translate(model, bike_pos);
        vec3 yaxis = {0,1,0};
        glm_rotate(model, DANGLE[bike.dir], yaxis);
        shader_set_mat4(sh,"model",(float*)model);

        // Bike color: bright cyan when alive, dark red when dead
        if(bike.alive)
            emissive(sh,0.6f,1.0f,1.0f,0.1f,0.6f,0.7f);
        else
            emissive(sh,0.6f,0.1f,0.1f,0.4f,0.0f,0.0f);
        mesh_draw(&bike_mesh);

        // ── Fire particles (additive blend, depth-write off) ───────
        {
            vec3 cam_right = {camera.right[0], camera.right[1], camera.right[2]};
            vec3 cam_up    = {0.0f, 1.0f, 0.0f}; // world-up for stable flames

            int pf = particles_build(cam_right, cam_up);
            pmesh_update(&part_mesh, part_buf, pf);

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE); // additive — fire glows
            glDepthMask(GL_FALSE);             // don't write to depth buffer

            shader_use(psh);
            shader_set_mat4(psh, "view",       (float*)view);
            shader_set_mat4(psh, "projection", (float*)proj);
            pmesh_draw(&part_mesh);

            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
            shader_use(sh); // restore scene shader
        }

        glfwSwapBuffers(win);
        glfwPollEvents();
    }

    mesh_free(&sea); mesh_free(&arena); mesh_free(&arena_acc);
    mesh_free(&pillars); mesh_free(&pillar_acc);
    mesh_free(&bike_mesh); dynmesh_free(&trail_mesh);
    pmesh_free(&part_mesh);
    glDeleteProgram(sh); glDeleteProgram(psh);
    glfwTerminate();
    return 0;
}
