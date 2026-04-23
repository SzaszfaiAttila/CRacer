#include "particles.h"
#include "config.h"
#include "bike.h"
#include <math.h>
#include <GLFW/glfw3.h>

float part_buf[PART_BUF_FLOATS];

/* ── Pillar top centres ──────────────────────────────────────────────────── */
static const float PTOP[4][2] = {
    {-PILLAR_DIST,-PILLAR_DIST},
    { PILLAR_DIST,-PILLAR_DIST},
    {-PILLAR_DIST, PILLAR_DIST},
    { PILLAR_DIST, PILLAR_DIST},
};

/* ── Pillar ambient particles ────────────────────────────────────────────── */
typedef struct {
    float x, y, z;
    float vx, vy, vz;
    float life, max_life, size;
    int   pillar;
} Particle;

static Particle particles[TOTAL_PARTS];

/* ── Death explosion particles ───────────────────────────────────────────── */
typedef struct {
    float x, y, z;
    float vx, vy, vz;
    float life, max_life, size;
    float r, g, b;
} DeathParticle;

static DeathParticle death_particles[MAX_DEATH_PARTICLES];
static int death_next_free = 0;
static const float GRAVITY = 24.0f;

/* ── Dash spark particles ────────────────────────────────────────────────── */
typedef struct {
    float x, y, z;
    float vx, vy, vz;
    float life, max_life, size;
    float r, g, b;
} DashSpark;

static DashSpark  dash_sparks[MAX_DASH_SPARKS];
static int        dash_next_free = 0;
static unsigned int dash_seed    = 0xDA5EEDu;

/* ── LCG ─────────────────────────────────────────────────────────────────── */
static float lcg_rand(unsigned int *s) {
    *s = (*s) * 1664525u + 1013904223u;
    return (float)(*s & 0x7FFFFFFF) / (float)0x7FFFFFFF;
}

/* ── Pillar particle helpers ─────────────────────────────────────────────── */
static void particle_spawn(Particle *p, int pillar_idx, unsigned int *seed) {
    float r = lcg_rand(seed)*2.0f, theta = lcg_rand(seed)*6.2832f;
    p->x        = PTOP[pillar_idx][0] + r*cosf(theta);
    p->y        = PILLAR_HEIGHT + 3.5f;
    p->z        = PTOP[pillar_idx][1] + r*sinf(theta);
    p->vx       = (lcg_rand(seed)-0.5f)*1.8f;
    p->vy       = 3.5f + lcg_rand(seed)*7.0f;
    p->vz       = (lcg_rand(seed)-0.5f)*1.8f;
    p->max_life = 0.9f + lcg_rand(seed)*1.4f;
    p->life     = p->max_life * lcg_rand(seed);
    p->size     = 1.4f + lcg_rand(seed)*1.2f;
    p->pillar   = pillar_idx;
}

/* ── Init ────────────────────────────────────────────────────────────────── */
void particles_init(void) {
    static const unsigned int seeds[4] = {
        0xDEADBEEF, 0xCAFEBABE, 0x12345678, 0xABCDEF01
    };
    for (int i = 0; i < TOTAL_PARTS; i++) {
        int pillar = i / PARTS_PER_PILLAR;
        unsigned int seed = seeds[pillar] + (unsigned int)(i % PARTS_PER_PILLAR) * 6271u;
        particle_spawn(&particles[i], pillar, &seed);
    }
    for (int i = 0; i < MAX_DEATH_PARTICLES; i++) death_particles[i].life = 0.0f;
    death_next_free = 0;

    for (int i = 0; i < MAX_DASH_SPARKS; i++) dash_sparks[i].life = 0.0f;
    dash_next_free = 0;
}

/* ── Update ──────────────────────────────────────────────────────────────── */
void particles_update(float dt) {
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
        float noise = sinf(t*3.1f + i*0.37f);
        p->vx += noise * 0.9f * dt;
        p->vz += cosf(t*2.7f + i*0.53f) * 0.9f * dt;
        p->vx *= (1.0f - 1.5f*dt);
        p->vz *= (1.0f - 1.5f*dt);
        p->vy -= 0.4f * dt;
        if (p->vy < 0.5f) p->vy = 0.5f;
        p->x += p->vx * dt;
        p->y += p->vy * dt;
        p->z += p->vz * dt;
    }
}

void particles_update_death(float dt) {
    for (int i = 0; i < MAX_DEATH_PARTICLES; i++) {
        DeathParticle *p = &death_particles[i];
        if (p->life <= 0.0f) continue;
        p->life -= dt;
        if (p->life <= 0.0f) continue;

        p->x += p->vx * dt;
        p->y += p->vy * dt;
        p->z += p->vz * dt;
        p->vy -= GRAVITY * dt;

        float air_drag = 1.0f - 1.5f * dt;
        p->vx *= air_drag;
        p->vz *= air_drag;

        if (p->y <= ARENA_TOP) {
            p->y  = ARENA_TOP;
            p->vy = 1.0f;
            float gf = 1.0f - 0.0f * dt;
            p->vx *= gf;
            p->vz *= gf;
            if (fabsf(p->vx) < 0.0f && fabsf(p->vz) < 0.0f) {
                p->vx = 0.0f; p->vz = 0.0f;
            }
        }
    }
}

void particles_update_dash(float dt) {
    for (int i = 0; i < MAX_DASH_SPARKS; i++) {
        DashSpark *p = &dash_sparks[i];
        if (p->life <= 0.0f) continue;
        p->life -= dt;
        if (p->life <= 0.0f) continue;

        p->x += p->vx * dt;
        p->y += p->vy * dt;
        p->z += p->vz * dt;

        /* light upward float + gentle drag */
        p->vy -= 4.0f * dt;
        p->vx *= (1.0f - 2.5f * dt);
        p->vz *= (1.0f - 2.5f * dt);
    }
}

/* ── Spawn helpers ───────────────────────────────────────────────────────── */
void particles_spawn_death_cloud(float px, float pz, Direction dir,
                                 float speed,
                                 float base_r, float base_g, float base_b,
                                 float spawn_y) {
    float dx = DDX[dir], dz = DDZ[dir];
    unsigned int seed = 0xDEADCAFEu ^ ((unsigned int)(px * 100.0f + pz));

    for (int i = 0; i < DEATH_PARTS_PER_BIKE; i++) {
        int idx = death_next_free % MAX_DEATH_PARTICLES;
        DeathParticle *p = &death_particles[idx];

        float ang  = lcg_rand(&seed) * 6.2832f;
        float dist = lcg_rand(&seed) * 2.8f;
        p->x = px + cosf(ang) * dist;
        p->z = pz + sinf(ang) * dist;
        p->y = spawn_y + 1.2f + lcg_rand(&seed) * 2.2f;

        float main_spd = speed * (0.95f + lcg_rand(&seed) * 0.65f);
        float perp     = 6.0f + lcg_rand(&seed) * 10.0f;
        float px_perp  = -dz, pz_perp = dx;
        float spread   = (lcg_rand(&seed) - 0.5f) * perp;

        p->vx = dx * main_spd + px_perp * spread;
        p->vz = dz * main_spd + pz_perp * spread;
        p->vy = 4.0f + lcg_rand(&seed) * 6.0f;

        p->r = base_r * (0.85f + lcg_rand(&seed) * 0.45f);
        p->g = base_g * (0.85f + lcg_rand(&seed) * 0.45f);
        p->b = base_b * (0.85f + lcg_rand(&seed) * 0.45f);

        p->size     = 0.06f + lcg_rand(&seed) * 0.12f;
        p->life     = p->max_life = 4.0f;

        death_next_free++;
    }
}

void particles_emit_dash(float px, float py, float pz,
                         Direction dir,
                         float cr, float cg, float cb,
                         int rate_level, float dt) {
    if (rate_level <= 0) return;

    /* Emission rate:
         trickle (ready) = 18/s  — visible but understated
         burst   (active) = 60/s — dense shower of sparks                     */
    float rate = (rate_level == 2) ? 60.0f : 18.0f;
    float prob  = rate * dt;

    /* Cap per-call count to avoid frame spikes; probability loop handles rate */
    int max_count = (rate_level == 2) ? 8 : 4;
    int count = 0;

    while (count < max_count && lcg_rand(&dash_seed) < prob) {
        prob -= 1.0f;
        count++;

        int idx = dash_next_free % MAX_DASH_SPARKS;
        DashSpark *p = &dash_sparks[idx];

        /* Spawn behind and around the bike — wider spread when bursting */
        float bx = -DDX[dir], bz_ = -DDZ[dir];
        float off      = 1.0f + lcg_rand(&dash_seed) * (rate_level == 2 ? 2.0f : 1.2f);
        float spread_r = (rate_level == 2) ? 1.0f : 0.45f;

        p->x = px + bx * off + (lcg_rand(&dash_seed) - 0.5f) * spread_r;
        p->z = pz + bz_* off + (lcg_rand(&dash_seed) - 0.5f) * spread_r;
        p->y = py + 0.2f + lcg_rand(&dash_seed) * (rate_level == 2 ? 1.2f : 0.7f);

        /* Velocity: kick backward + wild scatter when bursting */
        float spd = (rate_level == 2)
                  ? (5.0f + lcg_rand(&dash_seed) * 12.0f)
                  : (2.5f + lcg_rand(&dash_seed) * 5.0f);

        float scatter = (rate_level == 2) ? 6.0f : 2.5f;
        p->vx = bx * spd + (lcg_rand(&dash_seed) - 0.5f) * scatter;
        p->vz = bz_* spd + (lcg_rand(&dash_seed) - 0.5f) * scatter;
        p->vy = (rate_level == 2)
              ? (2.5f + lcg_rand(&dash_seed) * 7.0f)
              : (1.0f + lcg_rand(&dash_seed) * 3.5f);

        /* Brightness */
        float bright = (rate_level == 2) ? (1.0f + lcg_rand(&dash_seed) * 0.6f)
                                         : (0.55f + lcg_rand(&dash_seed) * 0.4f);
        p->r = fminf(cr * bright, 1.5f);
        p->g = fminf(cg * bright, 1.5f);
        p->b = fminf(cb * bright, 1.5f);

        /* Size — burst sparks are noticeably larger */
        p->size = (rate_level == 2)
                ? (0.10f + lcg_rand(&dash_seed) * 0.16f)
                : (0.06f + lcg_rand(&dash_seed) * 0.08f);

        /* Lifetime — longer so the trail behind the bike is more visible */
        p->max_life = (rate_level == 2)
                    ? (0.55f + lcg_rand(&dash_seed) * 0.55f)   /* 0.55–1.10 s */
                    : (0.40f + lcg_rand(&dash_seed) * 0.45f);  /* 0.40–0.85 s */
        p->life = p->max_life;

        dash_next_free++;
    }
}

/* ── Build vertex buffer ─────────────────────────────────────────────────── */
int particles_build(vec3 cam_right, vec3 cam_up) {
    int off = 0;

    /* Pillar particles */
    for (int i = 0; i < TOTAL_PARTS; i++) {
        Particle *p = &particles[i];
        float ratio = p->life / p->max_life;
        float r, g, b, a;
        if (ratio > 0.65f) {
            float t = (ratio - 0.65f) / 0.35f;
            r = 0.5f + t*0.5f; g = 0.7f + t*0.3f; b = 1.0f;
        } else if (ratio > 0.25f) {
            float t = (ratio - 0.25f) / 0.40f;
            r = 0.05f + t*0.45f; g = 0.25f + t*0.45f; b = 1.0f;
        } else {
            float t = ratio / 0.25f;
            r = 0.0f; g = t*0.1f; b = 0.4f + t*0.6f;
        }
        a = ratio * 0.85f;
        float hs = p->size * (0.4f + ratio*0.6f);
        float rx = cam_right[0]*hs, ry = cam_right[1]*hs, rz = cam_right[2]*hs;
        float ux = cam_up[0]*hs,   uy = cam_up[1]*hs,   uz = cam_up[2]*hs;
        float cx = p->x, cy = p->y, cz = p->z;
        float v[4][3] = {
            {cx-rx-ux, cy-ry-uy, cz-rz-uz},
            {cx+rx-ux, cy+ry-uy, cz+rz-uz},
            {cx+rx+ux, cy+ry+uy, cz+rz+uz},
            {cx-rx+ux, cy-ry+uy, cz-rz+uz},
        };
        int idx[6] = {0,1,2,0,2,3};
        for (int k = 0; k < 6; k++) {
            float *vv = v[idx[k]];
            part_buf[off++]=vv[0]; part_buf[off++]=vv[1]; part_buf[off++]=vv[2];
            part_buf[off++]=r;     part_buf[off++]=g;     part_buf[off++]=b;
            part_buf[off++]=a;
        }
    }

    /* Death particles */
    for (int i = 0; i < MAX_DEATH_PARTICLES; i++) {
        DeathParticle *p = &death_particles[i];
        if (p->life <= 0.0f) continue;

        float ratio = p->life / p->max_life;
        float r, g, b, a;
        if (ratio > 0.6f) {
            r = p->r * (0.75f + ratio*0.65f);
            g = p->g * (0.75f + ratio*0.65f);
            b = p->b * (0.75f + ratio*0.65f);
        } else {
            float fade = ratio / 0.6f;
            r = p->r * fade * 0.85f;
            g = p->g * fade * 0.85f;
            b = p->b * fade * 0.85f;
        }
        a = ratio * 0.96f;

        float hs = p->size * (0.35f + ratio*0.75f);
        float rx = cam_right[0]*hs, ry = cam_right[1]*hs, rz = cam_right[2]*hs;
        float ux = cam_up[0]*hs,   uy = cam_up[1]*hs,   uz = cam_up[2]*hs;
        float cx = p->x, cy = p->y, cz = p->z;
        float v[4][3] = {
            {cx-rx-ux, cy-ry-uy, cz-rz-uz},
            {cx+rx-ux, cy+ry-uy, cz+rz-uz},
            {cx+rx+ux, cy+ry+uy, cz+rz+uz},
            {cx-rx+ux, cy-ry+uy, cz-rz+uz},
        };
        int idx[6] = {0,1,2,0,2,3};
        for (int k = 0; k < 6; k++) {
            float *vv = v[idx[k]];
            part_buf[off++]=vv[0]; part_buf[off++]=vv[1]; part_buf[off++]=vv[2];
            part_buf[off++]=r;     part_buf[off++]=g;     part_buf[off++]=b;
            part_buf[off++]=a;
        }
    }

    /* Dash spark particles */
    for (int i = 0; i < MAX_DASH_SPARKS; i++) {
        DashSpark *p = &dash_sparks[i];
        if (p->life <= 0.0f) continue;

        float ratio = p->life / p->max_life;
        float a = ratio * 0.92f;

        /* Bright core fades to dim as spark ages */
        float br = (ratio > 0.5f) ? 1.0f : (ratio / 0.5f);
        float r  = p->r * br;
        float g  = p->g * br;
        float b  = p->b * br;

        float hs = p->size * (0.5f + ratio * 0.5f);
        float rx = cam_right[0]*hs, ry = cam_right[1]*hs, rz = cam_right[2]*hs;
        float ux = cam_up[0]*hs,   uy = cam_up[1]*hs,   uz = cam_up[2]*hs;
        float cx = p->x, cy = p->y, cz = p->z;
        float v[4][3] = {
            {cx-rx-ux, cy-ry-uy, cz-rz-uz},
            {cx+rx-ux, cy+ry-uy, cz+rz-uz},
            {cx+rx+ux, cy+ry+uy, cz+rz+uz},
            {cx-rx+ux, cy-ry+uy, cz-rz+uz},
        };
        int idx2[6] = {0,1,2,0,2,3};
        for (int k = 0; k < 6; k++) {
            float *vv = v[idx2[k]];
            part_buf[off++]=vv[0]; part_buf[off++]=vv[1]; part_buf[off++]=vv[2];
            part_buf[off++]=r;     part_buf[off++]=g;     part_buf[off++]=b;
            part_buf[off++]=a;
        }
    }

    return off;
}
