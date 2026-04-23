#include "bike.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

const float DDX[4]    = {  0.0f,  1.0f,  0.0f, -1.0f };
const float DDZ[4]    = { -1.0f,  0.0f,  1.0f,  0.0f };
const float DANGLE[4] = {  0.0f, -1.5707963f, 3.1415926f, 1.5707963f };

float trail_w = 0.1f;

/* ── Internal helpers ──────────────────────────────────────────────────────── */

/* Point + uniform radius vs axis-aligned segment — used by AI lookahead      */
static int seg_hit(float px, float pz, float hw,
                   float ax, float az, float bx, float bz) {
    if (fabsf(ax - bx) < 0.001f) {          /* N/S segment (constant X) */
        float mn = fminf(az, bz), mx = fmaxf(az, bz);
        return fabsf(px - ax) < hw && pz > mn - hw && pz < mx + hw;
    } else {                                  /* E/W segment (constant Z) */
        float mn = fminf(ax, bx), mx = fmaxf(ax, bx);
        return fabsf(pz - az) < hw && px > mn - hw && px < mx + hw;
    }
}

/* Axis-aligned bounding box vs axis-aligned segment — used by real collision.
   (hcx,hcz): world-space centre of the hitbox.
   hx_ext   : half-extent along world X.
   hz_ext   : half-extent along world Z.
   Trail segment is thin (TRAIL_PHYS_HW on each side of its centreline).      */
static int seg_box(float hcx, float hcz, float hx_ext, float hz_ext,
                   float ax, float az, float bx, float bz) {
    if (fabsf(ax - bx) < 0.001f) {          /* N/S segment (constant X) */
        float mn = fminf(az, bz), mx = fmaxf(az, bz);
        /* X: hitbox must overlap the trail's thin X band                    */
        /* Z: hitbox must overlap the segment's Z extent                     */
        return (fabsf(hcx - ax) < TRAIL_PHYS_HW + hx_ext) &&
               (hcz + hz_ext > mn) && (hcz - hz_ext < mx);
    } else {                                  /* E/W segment (constant Z) */
        float mn = fminf(ax, bx), mx = fmaxf(ax, bx);
        return (fabsf(hcz - az) < TRAIL_PHYS_HW + hz_ext) &&
               (hcx + hx_ext > mn) && (hcx - hx_ext < mx);
    }
}

/* ── Lifecycle ─────────────────────────────────────────────────────────────── */
void bike_reset(Bike *b) {
    memset(b, 0, sizeof(Bike));
    b->x              = 0.0f;
    b->z              = 48.0f;
    b->y              = ARENA_TOP;
    b->vy             = 0.0f;
    b->falling        = 0;
    b->fall_start_x   = 0.0f;
    b->fall_start_z   = 0.0f;
    b->trail_end_dist = 1e30f;
    b->dir            = DIR_NORTH;
    b->alive          = 1;
    b->wp_x[0]       = b->x;
    b->wp_z[0]       = b->z;
    b->wp_dist[0]    = 0.0f;
    b->wp_count       = 1;
    b->total_dist     = 0.0f;

    b->speed_mult    = 1.0f;
    b->dash_timer    = 0.0f;
    b->dash_cooldown = 0.0f;
}

void bike_activate_dash(Bike *b) {
    if (!b->alive || b->falling)              return;
    if (b->dash_timer > 0.0f)                 return;  /* already dashing  */
    if (b->dash_cooldown > 0.0f)              return;  /* still recharging */
    b->dash_timer = DASH_DURATION;
    b->speed_mult = DASH_SPEED_MULT;
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
    float spd = BIKE_SPEED * b->speed_mult;
    b->x          += DDX[b->dir] * spd * dt;
    b->z          += DDZ[b->dir] * spd * dt;
    b->total_dist += spd * dt;
}

/* ── Collision ─────────────────────────────────────────────────────────────── */
int bike_trail_point_hits(Bike *b, float px, float pz, float radius, int use_grace) {
    if (b->wp_count < 1) return 0;

    float tail_dist = b->total_dist - TRAIL_LENGTH;
    float end_dist  = b->alive ?
                      (use_grace ? (b->total_dist - GRACE_DIST) : b->total_dist)
                    : b->total_dist;

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
            bx = b->falling ? b->fall_start_x : b->x;
            bz = b->falling ? b->fall_start_z : b->z;
            ed = (b->falling || !b->alive) ? b->trail_end_dist : b->total_dist;
        }

        float cs = (tail_dist > sd) ? tail_dist : sd;
        float ce = (end_dist  < ed) ? end_dist  : ed;
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

/* ── Box-based collision check ───────────────────────────────────────────────
   Uses the HITBOX_* constants from config.h to define the bike's collision
   rectangle, which is always axis-aligned because bikes only travel in four
   cardinal directions.

   World-space hitbox centre (hcx, hcz) is derived from the bike's pivot
   position by applying the hitbox offsets in local space and rotating to world:

     hcx = bike.x  -  HITBOX_OFFSET_Z * DDX[dir]  -  HITBOX_OFFSET_X * DDZ[dir]
     hcz = bike.z  -  HITBOX_OFFSET_Z * DDZ[dir]  +  HITBOX_OFFSET_X * DDX[dir]

   (Local +Z = backward = opposite of travel, so we negate OFFSET_Z.)

   World half-extents depend on whether the bike travels N/S or E/W:
     - Perpendicular axis gets HITBOX_HALF_W.
     - Travel axis gets HITBOX_HALF_L.
   Using |DDX| and |DDZ| to select which axis is which.                        */
int bike_trail_box_hits(Bike *b, float px, float pz, Direction dir, int use_grace) {
    if (b->wp_count < 1) return 0;

    float adx = fabsf(DDX[dir]);   /* 1 for E/W travel, 0 for N/S */
    float adz = fabsf(DDZ[dir]);   /* 1 for N/S travel, 0 for E/W */

    /* Hitbox world-space centre */
    float hcx = px - HITBOX_OFFSET_Z * DDX[dir] - HITBOX_OFFSET_X * DDZ[dir];
    float hcz = pz - HITBOX_OFFSET_Z * DDZ[dir] + HITBOX_OFFSET_X * DDX[dir];

    /* World half-extents: W is perpendicular to travel, L is along travel */
    float hx_ext = HITBOX_HALF_W * adz + HITBOX_HALF_L * adx;
    float hz_ext = HITBOX_HALF_L * adz + HITBOX_HALF_W * adx;

    float tail_dist = b->total_dist - TRAIL_LENGTH;
    float end_dist  = b->alive
                    ? (use_grace ? (b->total_dist - GRACE_DIST) : b->total_dist)
                    : b->total_dist;

    int n = b->wp_count;
    for (int i = 0; i < n; i++) {
        float ax = b->wp_x[i], az = b->wp_z[i], sd = b->wp_dist[i];
        float bx, bz, ed;

        if (i + 1 < n) {
            bx = b->wp_x[i+1]; bz = b->wp_z[i+1]; ed = b->wp_dist[i+1];
        } else {
            bx = b->falling ? b->fall_start_x : b->x;
            bz = b->falling ? b->fall_start_z : b->z;
            ed = (b->falling || !b->alive) ? b->trail_end_dist : b->total_dist;
        }

        float cs = (tail_dist > sd) ? tail_dist : sd;
        float ce = (end_dist  < ed) ? end_dist  : ed;
        if (cs >= ce) continue;

        float len = ed - sd;
        if (len < 0.0001f) continue;

        float ts = (cs - sd) / len, te = (ce - sd) / len;
        float cax = ax + (bx - ax) * ts, caz = az + (bz - az) * ts;
        float cbx = ax + (bx - ax) * te, cbz = az + (bz - az) * te;

        if (seg_box(hcx, hcz, hx_ext, hz_ext, cax, caz, cbx, cbz)) return 1;
    }
    return 0;
}

int bike_check_collision_multi(Bike *b, Bike **all, int n) {
    float px = b->x, pz = b->z;

    if (fabsf(px) > ARENA_HALF - 0.4f || fabsf(pz) > ARENA_HALF - 0.4f) {
        if (!b->falling) {
            b->falling        = 1;
            b->fall_start_x   = b->x;
            b->fall_start_z   = b->z;
            b->vy             = -2.0f;
            b->trail_end_dist = b->total_dist;
        }
        return 0;
    }

    /* Use the hitbox-based test for every trail in the scene so that the red
       debug box (SHOW_HITBOX) and the actual collision zone are identical.    */
    for (int i = 0; i < n; i++) {
        int use_grace = (all[i] == b);
        if (bike_trail_box_hits(all[i], px, pz, b->dir, use_grace))
            return 1;
    }
    return 0;
}

void bike_update(Bike *b, float dt, Bike **all, int n) {
    if (!b->alive) {
        /* Keep tail fading at base speed regardless of speed_mult */
        b->total_dist += BIKE_SPEED * dt;
        return;
    }

    /* ── Dash timer tick ─────────────────────────────────────────────── */
    if (b->dash_timer > 0.0f) {
        b->dash_timer -= dt;
        if (b->dash_timer <= 0.0f) {
            b->dash_timer    = 0.0f;
            b->speed_mult    = 1.0f;
            b->dash_cooldown = DASH_COOLDOWN_DUR;
        }
    } else if (b->dash_cooldown > 0.0f) {
        b->dash_cooldown -= dt;
        if (b->dash_cooldown < 0.0f) b->dash_cooldown = 0.0f;
    }

    if (b->falling) {
        float spd = BIKE_SPEED * b->speed_mult;
        b->vy -= 32.0f * dt;
        b->y  += b->vy * dt;

        if (b->y > ARENA_BASE + 4.0f) {
            b->x += DDX[b->dir] * spd * 0.92f * dt;
            b->z += DDZ[b->dir] * spd * 0.92f * dt;
        }

        b->total_dist += spd * dt;

        if (b->y <= ARENA_BASE + 4.0f) {
            b->y     = ARENA_BASE + 4.0f;
            b->alive = 0;
        }
        return;
    }

    bike_move(b, dt);
    if (bike_check_collision_multi(b, all, n)) {
        b->alive          = 0;
        b->trail_end_dist = b->total_dist;
    }
}

/* ── Trail geometry ─────────────────────────────────────────────────────────── */
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

        if (fabsf(ax - bx) < 0.001f) {           /* N/S segment */
            float z0s = fminf(start_z, bz), z1s = fmaxf(start_z, bz);
            if (z1s - z0s > 0.01f)               /* skip zero-length */
                off = add_box(buf, off, ax-hw, y0, z0s, ax+hw, y1, z1s);
        } else {                                   /* E/W segment */
            float x0s = fminf(start_x, bx), x1s = fmaxf(start_x, bx);
            if (x1s - x0s > 0.01f)
                off = add_box(buf, off, x0s, y0, az-hw, x1s, y1, az+hw);
        }
    }
    return off;
}