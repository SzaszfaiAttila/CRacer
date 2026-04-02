#include "bike.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

const float DDX[4]    = {  0.0f,  1.0f,  0.0f, -1.0f };
const float DDZ[4]    = { -1.0f,  0.0f,  1.0f,  0.0f };
const float DANGLE[4] = {  0.0f, -1.5707963f, 3.1415926f, 1.5707963f };

float trail_w = 0.1f;

// ── Internal helpers ──────────────────────────────────────────────────────────
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
    b->y           = ARENA_TOP;
    b->vy          = 0.0f;
    b->falling     = 0;
    b->fall_start_x = 0.0f;
    b->fall_start_z = 0.0f;
    b->trail_end_dist = 1e30f;
    b->dir         = DIR_NORTH;
    b->alive       = 1;
    b->wp_x[0]    = b->x;
    b->wp_z[0]    = b->z;
    b->wp_dist[0] = 0.0f;
    b->wp_count    = 1;
    b->total_dist  = 0.0f;
}

void bike_turn_left(Bike *b) {
    if (!b->alive || b->falling) return;
    b->wp_x[b->wp_count]    = b->x;
    b->wp_z[b->wp_count]    = b->z;
    b->wp_dist[b->wp_count] = b->total_dist;
    b->wp_count++;
    b->dir = (Direction)((b->dir + 3) % 4);
}

void bike_turn_right(Bike *b) {
    if (!b->alive || b->falling) return;
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
    float end_dist  = b->alive ?
                      (use_grace ? (b->total_dist - GRACE_DIST) : b->total_dist)
                    : b->total_dist;   // dead bikes use full length for normal fade speed

    float hw = TRAIL_PHYS_HW + radius;

    int n = b->wp_count;
    for (int i = 0; i < n; i++) {
        float ax = b->wp_x[i], az = b->wp_z[i], sd = b->wp_dist[i];
        float bx, bz, ed;

        if (i + 1 < n) {
            bx = b->wp_x[i+1];
            bz = b->wp_z[i+1];
            ed = b->wp_dist[i+1];
        } else {
            // Freeze endpoint: don't let the trail follow the bike into the air or past death
            bx = b->falling ? b->fall_start_x : b->x;
            bz = b->falling ? b->fall_start_z : b->z;
            ed = (b->falling || !b->alive) ? b->trail_end_dist : b->total_dist;
        }

        float cs = (tail_dist > sd) ? tail_dist : sd;
        float ce = (end_dist < ed) ? end_dist : ed;
        if (cs >= ce) continue;

        float len = ed - sd;
        if (len < 0.0001f) continue;

        float ts = (cs - sd) / len;
        float te = (ce - sd) / len;
        float cax = ax + (bx - ax) * ts;
        float caz = az + (bz - az) * ts;
        float cbx = ax + (bx - ax) * te;
        float cbz = az + (bz - az) * te;

        if (seg_hit(px, pz, hw, cax, caz, cbx, cbz)) return 1;
    }
    return 0;
}

int bike_check_collision_multi(Bike *b, Bike **all, int n) {
    float px = b->x, pz = b->z;

    // Delayed edge detection — bike must go further over the edge before falling
    if (fabsf(px) > ARENA_HALF - 0.4f || fabsf(pz) > ARENA_HALF - 0.4f) {
        if (!b->falling) {
            b->falling = 1;
            b->fall_start_x = b->x;
            b->fall_start_z = b->z;
            b->vy = -2.0f;
            b->trail_end_dist = b->total_dist; // freeze trail at the edge
        }
        return 0;
    }

    // Normal trail collisions
    for (int i = 0; i < n; i++) {
        int use_grace = (all[i] == b);
        if (bike_trail_point_hits(all[i], px, pz, COLLISION_R, use_grace))
            return 1;
    }
    return 0;
}

void bike_update(Bike *b, float dt, Bike **all, int n) {
    if (!b->alive) {
        b->total_dist += BIKE_SPEED * dt;   // keep advancing so tail fades
        return;
    }

    if (b->falling) {
        b->vy -= 32.0f * dt;
        b->y  += b->vy * dt;

        if (b->y > ARENA_BASE + 4.0f) {
            b->x += DDX[b->dir] * BIKE_SPEED * 0.92f * dt;
            b->z += DDZ[b->dir] * BIKE_SPEED * 0.92f * dt;
        }

        b->total_dist += BIKE_SPEED * dt;

        if (b->y <= ARENA_BASE + 4.0f) {
            b->y = ARENA_BASE + 4.0f;
            b->alive = 0;
        }
        return;
    }

    bike_move(b, dt);
    if (bike_check_collision_multi(b, all, n)) {
        b->alive = 0;
        b->trail_end_dist = b->total_dist; // freeze trail at death point
    }
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
            bx = b->wp_x[i+1];
            bz = b->wp_z[i+1];
            ed = b->wp_dist[i+1];
        } else {
            // Freeze endpoint: don't let the trail follow the bike into the air or past death
            bx = b->falling ? b->fall_start_x : b->x;
            bz = b->falling ? b->fall_start_z : b->z;
            ed = (b->falling || !b->alive) ? b->trail_end_dist : b->total_dist;
        }

        if (ed < tail_dist) continue;

        float start_x = ax, start_z = az;
        if (tail_dist > sd) {
            float t = (tail_dist - sd) / (ed - sd);
            start_x = ax + (bx - ax) * t;
            start_z = az + (bz - az) * t;
        }

        if (fabsf(ax - bx) < 0.001f) {
            float z0s = fminf(start_z, bz), z1s = fmaxf(start_z, bz);
            off = add_box(buf, off, ax-hw, y0, z0s, ax+hw, y1, z1s);
        } else {
            float x0s = fminf(start_x, bx), x1s = fmaxf(start_x, bx);
            off = add_box(buf, off, x0s, y0, az-hw, x1s, y1, az+hw);
        }
    }
    return off;
}
