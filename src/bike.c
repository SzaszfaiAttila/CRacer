#include "bike.h"
#include <math.h>
#include <string.h>

const float DDX[4]    = {  0.0f,  1.0f,  0.0f, -1.0f };
const float DDZ[4]    = { -1.0f,  0.0f,  1.0f,  0.0f };
const float DANGLE[4] = {  0.0f, -1.5707963f, 3.1415926f, 1.5707963f };

float trail_w = 0.1f;

// ── Internal helpers ──────────────────────────────────────────────────────────

// Returns 1 if (px,pz) is within hw of the axis-aligned segment (ax,az)→(bx,bz)
static int seg_hit(float px, float pz, float hw,
                   float ax, float az, float bx, float bz) {
    if (fabsf(ax - bx) < 0.001f) {            // N/S constant-X segment
        float mn = fminf(az, bz), mx = fmaxf(az, bz);
        return fabsf(px - ax) < hw && pz > mn - hw && pz < mx + hw;
    } else {                                   // E/W constant-Z segment
        float mn = fminf(ax, bx), mx = fmaxf(ax, bx);
        return fabsf(pz - az) < hw && px > mn - hw && px < mx + hw;
    }
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void bike_reset(Bike *b) {
    memset(b, 0, sizeof(Bike));
    b->x           = 0.0f;
    b->z           = 48.0f;
    b->dir         = DIR_NORTH;
    b->alive       = 1;
    b->wp_x[0]    = b->x;
    b->wp_z[0]    = b->z;
    b->wp_dist[0] = 0.0f;
    b->wp_count    = 1;
    b->total_dist  = 0.0f;
}

void bike_turn_left(Bike *b) {
    if (!b->alive) return;
    b->wp_x[b->wp_count]    = b->x;
    b->wp_z[b->wp_count]    = b->z;
    b->wp_dist[b->wp_count] = b->total_dist;
    b->wp_count++;
    b->dir = (Direction)((b->dir + 3) % 4);
}

void bike_turn_right(Bike *b) {
    if (!b->alive) return;
    b->wp_x[b->wp_count]    = b->x;
    b->wp_z[b->wp_count]    = b->z;
    b->wp_dist[b->wp_count] = b->total_dist;
    b->wp_count++;
    b->dir = (Direction)((b->dir + 1) % 4);
}

void bike_move(Bike *b, float dt) {
    if (!b->alive) return;
    b->x          += DDX[b->dir] * BIKE_SPEED * dt;
    b->z          += DDZ[b->dir] * BIKE_SPEED * dt;
    b->total_dist += BIKE_SPEED * dt;
}

// ── Collision ─────────────────────────────────────────────────────────────────

int bike_trail_point_hits(Bike *b, float px, float pz, float radius, int use_grace) {
    if (b->wp_count < 1) return 0;

    float tail_dist = b->total_dist - TRAIL_LENGTH;
    float end_dist  = use_grace ? (b->total_dist - GRACE_DIST) : b->total_dist;
    float hw        = TRAIL_PHYS_HW + radius;

    int n = b->wp_count;
    for (int i = 0; i < n; i++) {
        float ax = b->wp_x[i], az = b->wp_z[i], sd = b->wp_dist[i];
        float bx, bz, ed;
        if (i + 1 < n) {
            bx = b->wp_x[i+1]; bz = b->wp_z[i+1]; ed = b->wp_dist[i+1];
        } else {
            // Last (growing) segment: waypoint → current head
            bx = b->x; bz = b->z; ed = b->total_dist;
        }

        if (ed < tail_dist)  continue; // fully faded
        if (sd > end_dist)   continue; // in grace zone or beyond

        // Clip segment endpoints to [tail_dist, end_dist]
        float cs = (tail_dist > sd) ? tail_dist : sd;
        float ce = (end_dist  < ed) ? end_dist  : ed;
        if (cs >= ce) continue;

        float len = ed - sd;
        if (len < 0.0001f) continue;
        float ts = (cs - sd) / len, te = (ce - sd) / len;
        float cax = ax + (bx - ax) * ts, caz = az + (bz - az) * ts;
        float cbx = ax + (bx - ax) * te, cbz = az + (bz - az) * te;

        if (seg_hit(px, pz, hw, cax, caz, cbx, cbz)) return 1;
    }
    return 0;
}

int bike_check_collision_multi(Bike *b, Bike **all, int n) {
    float px = b->x, pz = b->z;

    // Arena boundary
    if (fabsf(px) > ARENA_HALF - COLLISION_R || fabsf(pz) > ARENA_HALF - COLLISION_R)
        return 1;

    for (int i = 0; i < n; i++) {
        // use_grace=1 for self (avoid false positive from own recent trail)
        // use_grace=0 for others (their full visible trail is solid)
        int use_grace = (all[i] == b);
        if (bike_trail_point_hits(all[i], px, pz, COLLISION_R, use_grace))
            return 1;
    }
    return 0;
}

void bike_update(Bike *b, float dt, Bike **all, int n) {
    if (!b->alive) return;
    bike_move(b, dt);
    if (bike_check_collision_multi(b, all, n))
        b->alive = 0;
}

// ── Trail geometry ────────────────────────────────────────────────────────────

int bike_build_trail(Bike *b, float *buf) {
    int   off       = 0;
    float y0        = ARENA_TOP;
    float y1        = ARENA_TOP + TRAIL_HEIGHT;
    float hw        = trail_w / 2.0f;
    float tail_dist = b->total_dist - TRAIL_LENGTH;

    int n = b->wp_count;
    for (int i = 0; i < n; i++) {
        float ax = b->wp_x[i], az = b->wp_z[i], sd = b->wp_dist[i];
        float bx, bz, ed;
        if (i + 1 < n) {
            bx = b->wp_x[i+1]; bz = b->wp_z[i+1]; ed = b->wp_dist[i+1];
        } else {
            bx = b->x; bz = b->z; ed = b->total_dist;
        }

        if (ed < tail_dist) continue;

        // Clip tail
        float start_x = ax, start_z = az;
        if (tail_dist > sd) {
            float t = (tail_dist - sd) / (ed - sd);
            start_x = ax + (bx - ax) * t;
            start_z = az + (bz - az) * t;
        }

        // Determine orientation from original (unclipped) endpoints
        if (fabsf(ax - bx) < 0.001f) {  // N/S segment
            float z0s = fminf(start_z, bz), z1s = fmaxf(start_z, bz);
            off = add_box(buf, off, ax-hw, y0, z0s, ax+hw, y1, z1s);
        } else {                          // E/W segment
            float x0s = fminf(start_x, bx), x1s = fmaxf(start_x, bx);
            off = add_box(buf, off, x0s, y0, az-hw, x1s, y1, az+hw);
        }
    }
    return off;
}